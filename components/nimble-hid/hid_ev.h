#pragma once

#include <stdint.h>

typedef enum {
	HIDEV_EVENT_KEY_DOWN = 0,
	HIDEV_EVENT_KEY_UP,
	HIDEV_EVENT_JOY_BUTTONDOWN,
	HIDEV_EVENT_JOY_BUTTONUP,
	HIDEV_EVENT_JOY_AXIS,
	HIDEV_EVENT_JOY_HAT,
	HIDEV_EVENT_MOUSE_BUTTONDOWN,
	HIDEV_EVENT_MOUSE_BUTTONUP,
	HIDEV_EVENT_MOUSE_MOTION,
	HIDEV_EVENT_MOUSE_WHEEL
} hidev_event_type_en;

typedef struct {
	int keycode;
} hidev_ev_key_t;

typedef struct {
	int pos;
} hidev_ev_joyaxis_t;

typedef struct {
	int pos;
} hidev_ev_joyhat_t;

typedef struct {
	int dx;
	int dy;
} hidev_ev_mouse_motion_t;

typedef struct {
	int d;
} hidev_ev_mouse_wheel_t;

typedef struct {
	hidev_event_type_en type;
	int device_id;
	int no;					//the how-many-th one of this type this event is about (button4 has no==4)
	union {
		hidev_ev_key_t key;
		hidev_ev_joyaxis_t joyaxis;
		hidev_ev_joyhat_t joyhat;
		hidev_ev_mouse_motion_t mouse_motion;
		hidev_ev_mouse_wheel_t mouse_wheel;
	};
} hid_ev_t;

typedef void(hidev_event_cb_t)(hid_ev_t *event);

typedef struct hidev_device_t hidev_device_t;

hidev_device_t *hidev_device_from_descriptor(uint8_t *descriptor, int descriptor_len, int device_id, hidev_event_cb_t *cb);
void hidev_parse_report(hidev_device_t *dev, uint8_t *report, int report_id);




