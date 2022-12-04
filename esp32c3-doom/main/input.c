#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nimble_hid.h"
#include "hid_ev.h"
#include "nvs_flash.h"

QueueHandle_t s_evq;

static void hid_cb(hid_ev_t *ev) {
	xQueueSend(s_evq, ev, 0);
}

void input_init() {
	esp_err_t ret = nvs_flash_init();
	if	(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	s_evq=xQueueCreate(32, sizeof(hid_ev_t));
	nimble_hid_start(hid_cb);
}

int input_get_event(hid_ev_t *ev) {
	return xQueueReceive(s_evq, ev, 0);
}

