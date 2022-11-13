idf_component_register(SRCS "misc.c" "peer.c" 
					"libusbhid/descr.c" "libusbhid/parse.c"
					"libusbhid/usage_tables.c" "libusbhid/data.c"
					"nimble_hid.c" "hid_ev.c"
					REQUIRES bt
                    INCLUDE_DIRS "." "libusbhid")
