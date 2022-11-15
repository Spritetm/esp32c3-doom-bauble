/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.	See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.	 See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "esp_log.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
/* BLE */
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_hs_pvcy.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "esp_log.h"
#include "hid_ev.h"
#include "blecent.h"

static const char *TAG = "ble_cent";
static int blecent_gap_event(struct ble_gap_event *event, void *arg);

typedef struct report_ep_t report_ep_t;
struct report_ep_t {
	int handle;
	int id;
	report_ep_t *next;
};
static report_ep_t *report_eps;

static hidev_device_t *s_hidev_dev;
hidev_event_cb_t *s_callback;

//static uint8_t peer_addr[6];

//State machine logic. blecent_next_state will be calles with these events to advance to the
//next state
#define EVT_RESET 0
#define EVT_WRITTEN 1
#define EVT_READ 2
#define EVT_DISC_COMPLETE 3
#define EVT_SYNCED 4
#define EVT_SCANNED 5
#define EVT_CONNECTED 6
#define EVT_DISCOVERED 7
#define EVT_ENC_CHANGED 8
const char *event_str[]={"EVT_RESET","EVT_WRITTEN","EVT_READ","EVT_DISC_COMPLETE","EVT_SYNCED",
		"EVT_SCANNED","EVT_CONNECTED","EVT_DISCOVERED", "EVT_ENC_CHANGED"};

static void blecent_next_state(int event, uint16_t conn_handle, int error, int attr_handle, struct os_mbuf *data);


void ble_store_config_init(void);

/**
 * Application callback.  Called when the attempt to subscribe to notifications
 * for the ANS Unread Alert Status characteristic has completed.
 */
static int blecent_on_write(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
	ESP_LOGI(TAG, "Write complete; status=%d conn_handle=%d "
				"attr_handle=%d", error->status, conn_handle, attr->handle);
	blecent_next_state(EVT_WRITTEN, conn_handle, error->status, 0, NULL);
	return 0;
}

struct os_mbuf *multiple_read_buf=NULL;

static int blecent_on_read_long(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
	ESP_LOGI(TAG, "Read complete; status=%d conn_handle=%d", error->status, conn_handle);
	if (error->status == 0) {
		if (multiple_read_buf) {
			os_mbuf_appendfrom(multiple_read_buf, attr->om, 0, os_mbuf_len(attr->om));
		} else {
			multiple_read_buf=attr->om;
			attr->om=NULL;
		}
	} else { //either (error->status == BLE_HS_EDONE) indicating success or something else
		if (error->status == BLE_HS_EDONE) {
			print_mbuf(multiple_read_buf);
		}
		blecent_next_state(EVT_READ, conn_handle, error->status, 0, multiple_read_buf);
		if (multiple_read_buf) {
			os_mbuf_free_chain(multiple_read_buf);
			multiple_read_buf=NULL;
		}
	}
	return 0;
}

static int blecent_on_read(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
	ESP_LOGI(TAG, "Read complete; status=%d conn_handle=%d", error->status, conn_handle);

	if (error->status == 0) {
		ESP_LOGI(TAG, " attr_handle=%d offset %d len=%d value=", attr->handle, attr->offset, os_mbuf_len(attr->om));
		print_mbuf(attr->om);
		blecent_next_state(EVT_READ, conn_handle, error->status, attr->handle, attr->om);
	} else {
		//callback for error
		blecent_next_state(EVT_READ, conn_handle, error->status, 0, NULL);
	}
	return 0;
}

/**
 * Called when service discovery of the specified peer has completed.
 */
static void blecent_on_disc_complete(const struct peer *peer, int status, void *arg) {
	blecent_next_state(EVT_DISCOVERED, peer->conn_handle, status, 0, NULL);
}

/**
 * Initiates the GAP general discovery procedure.
 */
static void
blecent_scan(void)
{
	uint8_t own_addr_type;
	struct ble_gap_disc_params disc_params={};
	int rc;

	/* Figure out address to use while advertising (no privacy for now) */
	rc = ble_hs_id_infer_auto(0, &own_addr_type);
	if (rc != 0) {
		ESP_LOGE(TAG, "error determining address type; rc=%d", rc);
		return;
	}

	// Tell the controller to filter duplicates; we don't want to process
	// repeated advertisements from the same device.
	disc_params.filter_duplicates = 1;
	// Perform a passive scan.	I.e., don't send follow-up scan requests to
	// each advertiser.
	disc_params.passive = 1;
	// Use defaults for the rest of the parameters.
	disc_params.itvl = 0;
	disc_params.window = 0;
	disc_params.filter_policy = 0;
	disc_params.limited = 0;

	rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &disc_params, blecent_gap_event, NULL);
	if (rc != 0) {
		ESP_LOGE(TAG, "Error initiating GAP discovery procedure; rc=%d", rc);
	}
}


/**
 * Connects to the sender of the specified advertisement of it looks
 * interesting.	 A device is "interesting" if it advertises connectability and
 * support for the Alert Notification service.
 */
static void blecent_connect_if_interesting(const struct ble_gap_disc_desc *disc) {
	uint8_t own_addr_type;

	printf("blecent_connect_if_interesting: addr %s evt_type %d\n", 
			addr_str(disc->addr.val), disc->event_type);

	//See if device is interesting:
	//The device has to be advertising connectability.
	if (disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND &&
			disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_DIR_IND) {
		printf("not advertising connectivity\n");
//		return;
	}

	struct ble_hs_adv_fields fields;
	int rc = ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data);
	if (rc != 0) {
		printf("ble_hs_adv_parse_fields failed\n");
		return;
	}

	//The device has to advertise support for the HID/HOGP service
	int has_service=0;
	printf("%d uuids\n", fields.num_uuids16);
	for (int i = 0; i < fields.num_uuids16; i++) {
		printf("UUID %04X\n", ble_uuid_u16(&fields.uuids16[i].u));
		if (ble_uuid_u16(&fields.uuids16[i].u) == BLECENT_SVC_HID_UUID) {
			has_service=1;
		}
	}
	if (!has_service) return;
	printf("Has HID/HOGP service\n");
	//Device is interesting! Connect.

	//Scanning must be stopped before a connection can be initiated.
	rc = ble_gap_disc_cancel();
	if (rc != 0) {
		ESP_LOGW(TAG, "Failed to cancel scan; rc=%d", rc);
		return;
	}

	// Figure out address to use for connect (no privacy for now)
	rc = ble_hs_id_infer_auto(0, &own_addr_type);
	if (rc != 0) {
		ESP_LOGE(TAG, "error determining address type; rc=%d", rc);
		return;
	}

	// Try to connect the the advertiser.  Allow 30 seconds (30000 ms) for timeout.
	rc = ble_gap_connect(own_addr_type, &disc->addr, 30000, NULL, blecent_gap_event, NULL);
	if (rc != 0) {
		ESP_LOGE(TAG, "Error: Failed to connect to device; addr_type=%d addr=%s; rc=%d",
					disc->addr.type, addr_str(disc->addr.val), rc);
		return;
	}
}

/**
 * The nimble host executes this callback when a GAP event occurs.	The
 * application associates a GAP event callback with each connection that is
 * established.	 blecent uses the same callback for all connections.
 *
 * @param event					The event being signalled.
 * @param arg					Application-specified argument; unused by
 *									blecent.
 *
 * @return						0 if the application successfully handled the
 *									event; nonzero on failure.	The semantics
 *									of the return code is specific to the
 *									particular GAP event being signalled.
 */
static int blecent_gap_event(struct ble_gap_event *event, void *arg)
{
	struct ble_gap_conn_desc desc;
	struct ble_hs_adv_fields fields;
	int rc;

	switch (event->type) {
	case BLE_GAP_EVENT_DISC:
		rc = ble_hs_adv_parse_fields(&fields, event->disc.data,  event->disc.length_data);
		if (rc == 0) {
			/* An advertisment report was received during GAP discovery. */
			print_adv_fields(&fields);
		}
			/* Try to connect to the advertiser if it looks interesting. */
			blecent_connect_if_interesting(&event->disc);
//		}
		return 0;
    case BLE_GAP_EVENT_EXT_DISC:
        /* An advertisment report was received during GAP discovery. */
		printf("ext report\n");
//        ext_print_adv_report(&event->disc);

//        blecent_connect_if_interesting(&event->disc);
        return 0;

	case BLE_GAP_EVENT_CONNECT:
		/* A new connection was established or a connection attempt failed. */
		blecent_next_state(EVT_CONNECTED, event->connect.conn_handle, event->connect.status, 0, NULL);

		return 0;

	case BLE_GAP_EVENT_DISCONNECT:
		/* Connection terminated. */
		ESP_LOGE(TAG, "disconnect; reason=%d ", event->disconnect.reason);
		print_conn_desc(&event->disconnect.conn);
		printf("\n");

		/* Forget about peer. */
		peer_delete(event->disconnect.conn.conn_handle);

		/* Resume scanning. */
		blecent_scan();
		return 0;

	case BLE_GAP_EVENT_DISC_COMPLETE:
		ESP_LOGI(TAG, "discovery complete; reason=%d", event->disc_complete.reason);
		return 0;

	case BLE_GAP_EVENT_ENC_CHANGE:
		/* Encryption has been enabled or disabled for this connection. */
		ESP_LOGI(TAG, "encryption change event; status=%d ", event->enc_change.status);
		rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
		assert(rc == 0);
		print_conn_desc(&desc);
		blecent_next_state(EVT_ENC_CHANGED, event->connect.conn_handle, event->connect.status, 0, NULL);
		return 0;

	case BLE_GAP_EVENT_NOTIFY_RX:
		/* Peer sent us a notification or indication. */
		ESP_LOGI(TAG, "received %s; conn_handle=%d attr_handle=%d attr_len=%d",
					event->notify_rx.indication ? "indication" : "notification",
					event->notify_rx.conn_handle,event->notify_rx.attr_handle,
					OS_MBUF_PKTLEN(event->notify_rx.om));
		/* Attribute data is contained in event->notify_rx.om. Use
		 * `os_mbuf_copydata` to copy the data received in notification mbuf */
		print_mbuf(event->notify_rx.om);

		if (s_hidev_dev) {
			report_ep_t *rep=report_eps;
			while (rep && rep->handle!=event->notify_rx.attr_handle) {
				rep=rep->next;
			}
			if (rep) {
				ESP_LOGI(TAG, "Parsing report for ID %d", rep->id);
				hidev_parse_report(s_hidev_dev, event->notify_rx.om->om_data, rep->id);
			}
		}
		return 0;

	case BLE_GAP_EVENT_MTU:
		ESP_LOGI(TAG, "mtu update event; conn_handle=%d cid=%d mtu=%d",
					event->mtu.conn_handle, event->mtu.channel_id, event->mtu.value);
		return 0;

	case BLE_GAP_EVENT_REPEAT_PAIRING:
		/* We already have a bond with the peer, but it is attempting to
		 * establish a new secure link.	 This app sacrifices security for
		 * convenience: just throw away the old bond and accept the new link.
		 */
		/* Delete the old bond. */
		rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
		assert(rc == 0);
		ble_store_util_delete_peer(&desc.peer_id_addr);
		/* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
		 * continue with the pairing operation.
		 */
		return BLE_GAP_REPEAT_PAIRING_RETRY;
	case BLE_GAP_EVENT_CONN_UPDATE:
		ESP_LOGI(TAG, "BLE_GAP_EVENT_CONN_UPDATE");
		return 0;
	case BLE_GAP_EVENT_L2CAP_UPDATE_REQ:
		ESP_LOGI(TAG, "BLE_GAP_EVENT_L2CAP_UPDATE_REQ");
		return 0;
	default:
		ESP_LOGW(TAG, "Unknown GAP event %d!", event->type);
		return 0;
	}
}

static void blecent_on_reset(int reason) {
	MODLOG_DFLT(ERROR, "Resetting state; reason=%d", reason);
	blecent_next_state(EVT_RESET, 0, reason, 0, NULL);
}

static void blecent_on_sync(void) {
	blecent_next_state(EVT_SYNCED, 0, 0, 0, NULL);
}

//Main state machine states
#define ST_INIT 0
#define ST_SYNCED 1
#define ST_SCANNED 2
#define ST_DISCOVERED 3
#define ST_CONNECTED 4
#define ST_READ_HIDMAP 5
#define ST_WRITE_PROTO 6
#define ST_WRITE_NOTIFY 7
#define ST_SECURITY_INIT 8
#define ST_READ_HIDINFO 9
#define ST_READ_RPT 10
#define ST_WR_RPT 11
#define ST_READ_RPT_META 12
#define ST_WRITE_NOTIFY_DONE 13
#define ST_REPORT_LOOP_START 14

const char *ble_state_str[]={"ST_INIT","ST_SYNCED","ST_SCANNED","ST_DISCOVERED","ST_CONNECTED",
		"ST_READ_HIDMAP", "ST_WRITE_PROTO", "ST_WRITE_NOTIFY", "ST_SECURITY_INIT", "ST_READ_HIDINFO",
		"ST_READ_RPT", "ST_WR_RPT", "ST_READ_RPT_META", "ST_WRITE_NOTIFY_DONE", "ST_REPORT_LOOP_START"};
int ble_state=ST_INIT;

//This state machine drives the entire discovery/read/connect logic
int nth=0;
static void blecent_next_state(int event, uint16_t conn_handle, int error, int attr_handle, struct os_mbuf *data) {
	ESP_LOGI(TAG, "* blecent_next_state on event %d (%s), current state %d (%s)", 
				event, event_str[event], ble_state, ble_state_str[ble_state]);
	struct peer *peer=NULL;
	if (conn_handle!=0) {
		peer=peer_find(conn_handle);
	}
	if (event==EVT_RESET) {
		ble_state=ST_INIT;
	} else if (event==EVT_SYNCED) {
		// Make sure we have proper identity address set (public preferred)
		int rc = ble_hs_util_ensure_addr(1); //1 for random, 0 for public address
		assert(rc == 0);
		//set privacy
//		rc=ble_hs_pvcy_rpa_config(NIMBLE_HOST_ENABLE_NRPA);
		rc=ble_hs_pvcy_rpa_config(1);
		if (rc!=0) {
			ESP_LOGE(TAG, "ble_hs_pvcy_rpa_config: rc=%d", rc);
		}
		//Begin scan
		blecent_scan();
		ble_state=ST_SCANNED;
	} else if (event==EVT_CONNECTED) {
		if (error==0) {
			ESP_LOGI(TAG, "Connection established ");
			struct ble_gap_conn_desc desc;
			int rc = ble_gap_conn_find(conn_handle, &desc);
			assert(rc == 0);
			print_conn_desc(&desc);
			printf("\n");

			rc = peer_add(conn_handle);
			if (rc==2) {
				ESP_LOGW(TAG, "Failed to add peer because already known; rc=%d\n", rc);
			} else if (rc != 0) {
				ESP_LOGE(TAG, "Failed to add peer; rc=%d\n", rc);
				return;
			}
			rc=ble_gap_security_initiate(conn_handle);
			if (rc!=0) {
				ESP_LOGE(TAG, "Failed to initiate bonding/pairing! rc=%d", rc);
			}
			ble_state=ST_SECURITY_INIT;
		} else {
			/* Connection attempt failed; resume scanning. */
			ESP_LOGE(TAG, "Error: Connection failed; status=%d",error);
			blecent_scan();
			ble_state=ST_SCANNED;
		}
	} else if (event==EVT_ENC_CHANGED && ble_state==ST_SECURITY_INIT) {
		int rc = peer_disc_all(conn_handle, blecent_on_disc_complete, NULL);
		if (rc != 0) {
			ESP_LOGE(TAG, "Failed to discover services; rc=%d", rc);
			return;
		}
		ble_state=ST_DISCOVERED;
	} else if (event==EVT_DISCOVERED) {
		if (error != 0) {
			ESP_LOGE(TAG, "Error: Service discovery failed; status=%d conn_handle=%d", error, conn_handle);
			ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
		} else {
			ESP_LOGI(TAG, "Service discovery complete; status=%d conn_handle=%d\n", error, conn_handle);
			const struct peer_chr *chr = peer_chr_find_uuid(peer,
				BLE_UUID16_DECLARE(BLECENT_SVC_HID_UUID),
				BLE_UUID16_DECLARE(BLECENT_CHR_PROTO_MODE_PT));
			if (chr == NULL) {
				ESP_LOGE(TAG, "Error: Peer lacks proto descriptor characteristic\n");
			}
			uint8_t value[2]={1,0}; //select Report Protocol
			int rc = ble_gattc_write_flat(conn_handle, chr->chr.val_handle, value, sizeof value, blecent_on_write, NULL);
			if (rc != 0) {
				ESP_LOGE(TAG, "Error: Failed to set proto; rc=%d", rc);
			}
			ble_state=ST_WRITE_PROTO;
		}
	} else if ((event==EVT_WRITTEN && ble_state==ST_WRITE_PROTO)) {
		const struct peer_chr *chr = peer_chr_find_uuid(peer,
					BLE_UUID16_DECLARE(BLECENT_SVC_HID_UUID),
					BLE_UUID16_DECLARE(BLECENT_CHR_REP_MAP_PT));
		if (!chr) {
			ESP_LOGE(TAG, "Error: Failed to find report map characteristic");
		}
		int rc = ble_gattc_read_long(peer->conn_handle, chr->chr.val_handle, 0, blecent_on_read_long, NULL);
		if (rc != 0) {
			ESP_LOGE(TAG, "Error: Failed to read characteristic; rc=%d",  rc);
		}
		ble_state=ST_READ_HIDMAP;
	} else if (event==EVT_READ && ble_state==ST_READ_HIDMAP) {
		ESP_LOGI(TAG, "Parsing hid report map of len %d", os_mbuf_len(data));
		uint8_t *repbuf=malloc(os_mbuf_len(data));
		os_mbuf_copydata(data, 0, os_mbuf_len(data), repbuf);
		s_hidev_dev=hidev_device_from_descriptor(repbuf, os_mbuf_len(data), 0, s_callback);
		free(repbuf);
		if (s_hidev_dev==NULL) {
			ESP_LOGE(TAG, "Error: Failed to parse hid report map");
		}
		const struct peer_chr *chr = peer_chr_find_uuid(peer,
					BLE_UUID16_DECLARE(BLECENT_SVC_HID_UUID),
					BLE_UUID16_DECLARE(BLECENT_CHR_HID_INFO_PT));
		if (chr == NULL) {
			ESP_LOGE(TAG, "Error: Peer doesn't support the HID INFO characteristic");
		}
		int rc = ble_gattc_read(peer->conn_handle, chr->chr.val_handle, blecent_on_read, NULL);
		if (rc != 0) {
			ESP_LOGE(TAG, "Error: Failed to read characteristic; rc=%d",  rc);
		}
		ble_state=ST_READ_HIDINFO;
	} else if (event==EVT_READ && ble_state==ST_READ_HIDINFO) {
		nth=0;
		ble_state=ST_REPORT_LOOP_START;
		//recursively call into the start of the loop
		blecent_next_state(event, conn_handle, error, attr_handle, data);
	} else if (ble_state==ST_REPORT_LOOP_START) {
		const struct peer_dsc *dsc = peer_dsc_find_uuid_nth(peer,
			BLE_UUID16_DECLARE(BLECENT_SVC_HID_UUID),
			BLE_UUID16_DECLARE(BLECENT_CHR_REP_PT),
			BLE_UUID16_DECLARE(BLECENT_DSC_REP_REF), nth);
		if (dsc == NULL) {
			ESP_LOGE(TAG, "Error: Peer doesn't support the report reference characteristic");
		}
		int rc = ble_gattc_read(peer->conn_handle, dsc->dsc.handle, blecent_on_read, NULL);
		if (rc != 0) {
			ESP_LOGE(TAG, "Error: Failed to read characteristic; rc=%d",  rc);
		}
		ble_state=ST_READ_RPT_META;
	} else if (ble_state==ST_READ_RPT_META) {
		const struct peer_chr *chr = peer_chr_find_uuid_nth(peer,
			BLE_UUID16_DECLARE(BLECENT_SVC_HID_UUID),
			BLE_UUID16_DECLARE(BLECENT_CHR_REP_PT), nth);
		if (chr == NULL) {
			ESP_LOGE(TAG, "Error: Peer doesn't support the report characteristic");
		}
		uint8_t rep_ref[2];
		os_mbuf_copydata(data, 0, 2, rep_ref);
		report_ep_t *rep=calloc(sizeof(report_ep_t), 1);
		rep->id=rep_ref[0];
		rep->handle=chr->chr.val_handle;
		rep->next=report_eps;
		report_eps=rep;

		int rc = ble_gattc_read(peer->conn_handle, chr->chr.val_handle, blecent_on_read, NULL);
		if (rc != 0) {
			ESP_LOGE(TAG, "Error: Failed to read characteristic; rc=%d",  rc);
		}
		ble_state=ST_READ_RPT;
	} else if (event==EVT_READ && ble_state==ST_READ_RPT) {
		//Some devices have multiple report characteristics.
		ESP_LOGI(TAG, "Attempting subscribe to %dth report uuid...", nth);
		const struct peer_dsc *dsc = peer_dsc_find_uuid_nth(peer,
			BLE_UUID16_DECLARE(BLECENT_SVC_HID_UUID),
			BLE_UUID16_DECLARE(BLECENT_CHR_REP_PT),
			BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16), nth);
		if (dsc == NULL) {
			ESP_LOGE(TAG, "Error: Peer lacks notify characteristic");
			ble_state=ST_WRITE_NOTIFY_DONE;
		} else {
			uint8_t value[2]={1,0};
			int rc = ble_gattc_write_flat(conn_handle, dsc->dsc.handle, value, sizeof value, blecent_on_write, NULL);
			if (rc != 0) {
				ESP_LOGE(TAG, "Error: Failed to set notify; rc=%d", rc);
				ble_state=ST_WRITE_NOTIFY_DONE;
			}
			nth++;
			ble_state=ST_REPORT_LOOP_START;
		}
	} else {
		//Note: state machine ends up here when done.
		ESP_LOGE(TAG, "Huh? Unknown state/event combo!");
	}
}


static void blecent_host_task(void *param) {
	ESP_LOGI(TAG, "BLE Host Task Started");
	/* This function will return only when nimble_port_stop() is executed */
	nimble_port_run();
	nimble_port_freertos_deinit();
}

void nimble_hid_start(hidev_event_cb_t *cb) {
	int rc;

	s_callback=cb;

	nimble_port_init();
	/* Configure the host. */
	ble_hs_cfg.reset_cb = blecent_on_reset;
	ble_hs_cfg.sync_cb = blecent_on_sync;
	ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
//	ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
	ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_DISP_YES_NO;
	ble_hs_cfg.sm_sc = 1;
	ble_hs_cfg.sm_bonding = 1;
	ble_hs_cfg.sm_mitm = 0;
	ble_hs_cfg.sm_our_key_dist = 1;
	ble_hs_cfg.sm_their_key_dist = 1;
	ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;
	ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;
	ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID; // Enable LTK + IRK
	ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID; // Enable LTK + IRK

	/* Initialize data structures to track connected peers. */
	rc = peer_init(MYNEWT_VAL(BLE_MAX_CONNECTIONS), 16, 64, 64);
	assert(rc == 0);

	/* Set the default device name. */
	rc = ble_svc_gap_device_name_set("xmas-bauble");
	assert(rc == 0);

	ble_store_config_init();

	nimble_port_freertos_init(blecent_host_task);
}
