#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include <stdint.h>
#include "assert.h"
#include "snd_c3.h"
#include "driver/sigmadelta.h"
#include "driver/timer.h"
#include "esp_log.h"

const char *TAG="sndc3";

#define RATE(x) (40000000/x)

#define BUFSZ 1024

static QueueHandle_t sampqueue;
static int8_t *pbuf;
static int ppos=BUFSZ; //start grabbing next buf

void timercb(void *arg) {
	if (ppos==BUFSZ) {
		if (xQueueReceiveFromISR(sampqueue, &pbuf, NULL)) {
			ppos=0;
		} else {
			//underrun
			sigmadelta_set_duty(0, 0);
			sigmadelta_set_duty(1, 0);
			return;
		}
	}
	sigmadelta_set_duty(0, pbuf[ppos]);
	sigmadelta_set_duty(1, -pbuf[ppos]);
	ppos++;
}

static int rate;
static snd_cb_t *snd_cb;

void snd_task(void *arg) {
	int8_t *buf_a=calloc(BUFSZ, 1);
	int8_t *buf_b=calloc(BUFSZ, 1);
	assert(buf_a);
	assert(buf_b);
	while(1) {
		snd_cb(buf_a, BUFSZ);
		xQueueSend(sampqueue, &buf_a, portMAX_DELAY);
		snd_cb(buf_b, BUFSZ);
		xQueueSend(sampqueue, &buf_b, portMAX_DELAY);
	}
}

void snd_init(int samprate, snd_cb_t *cb) {
	rate=samprate;
	snd_cb=cb;
	sampqueue=xQueueCreate(1, sizeof(uint8_t*));

	//Set up 2 sigma-delta converters, one for each GPIO
	sigmadelta_config_t sdconfig={
		.channel=SIGMADELTA_CHANNEL_0,
		.sigmadelta_duty=0,
		.sigmadelta_prescale=10,
		.sigmadelta_gpio=10,
	};
	ESP_ERROR_CHECK(sigmadelta_config(&sdconfig));
	sdconfig.sigmadelta_gpio=9;
	sdconfig.channel=SIGMADELTA_CHANNEL_1;
	ESP_ERROR_CHECK(sigmadelta_config(&sdconfig));

	//Set up timer for sample rate
	int group=0;
	int timer=0;
	/* Select and initialize basic parameters of the timer */
	timer_config_t config = {
		.divider = 2,
		.counter_dir = TIMER_COUNT_UP,
		.counter_en = TIMER_PAUSE,
		.alarm_en = TIMER_ALARM_EN,
		.auto_reload = true,
	}; // default clock source is APB
	timer_init(group, timer, &config);

	/* Timer's counter will initially start from value below.
	   Also, if auto_reload is set, this value will be automatically reload on alarm */
	timer_set_counter_value(group, timer, 0);

	/* Configure the alarm value and the interrupt on alarm. */
	timer_set_alarm_value(group, timer, RATE(samprate));
	timer_enable_intr(group, timer);
	timer_isr_callback_add(group, timer, timercb, NULL, 0);

	timer_start(group, timer);

	//Start task to handle sound stuff
	xTaskCreate(snd_task, "snd", 16*1024, NULL, 5, NULL);
}

