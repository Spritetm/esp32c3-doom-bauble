#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/adc.h"
#include "esp_timer.h"


static int b1, b2, i;

#define SAMPINTER_US (3*1000)
#define SAMP 10

//Given the index 3'bxyz, with x, y and z being 1 for a pressed button and 0 for a released one, this gives
//the expected ADC value. Note that this is calibrated instead of calculated, the values may be off for 
//your setup.
static int btnsamps[8]={4095, 1490, 2358, 1125, 3160, 1280, 1872, 1002};

static int buttons_pressed;

static int get_btn_from_adcval(int adc) {
	int hi=-1;
	int lo=-1;
	for (int i=0; i<8; i++) {
		if (adc>btnsamps[i] && (lo==-1 || btnsamps[lo]<btnsamps[i])) lo=i;
		if (adc<=btnsamps[i] && (hi==-1 || btnsamps[hi]>btnsamps[i])) hi=i;
	}
	if (hi==-1) hi=lo;
	if (lo==-1) lo=hi;
	int d1=btnsamps[hi]-adc;
	int d2=adc-btnsamps[lo];
//	printf("%d<%d<%d ", btnsamps[lo], adc, btnsamps[hi]);
	return (d1>d2) ? lo : hi;
}


static void btn_cb(void *arg) {
	int adc1=adc1_get_raw(ADC1_CHANNEL_1);
	int adc2=adc1_get_raw(ADC1_CHANNEL_2);
	if (i!=0) {
		//don't average over a change, as we'll average to some value in the middle leading to a wrong key interpretation
		if (abs(adc1-(b1/i))>100 || abs(adc2-(b2/i))>100) {
			b1=0; b2=0; i=0;
		}
	}
	b1+=adc1;
	b2+=adc2;
	i++;
	if (i==SAMP) {
		buttons_pressed=(get_btn_from_adcval(b1/SAMP)<<3)+ get_btn_from_adcval(b2/SAMP);
//		printf("Button %02x\n", bm);
		i=0; b1=0; b2=0;
	}
}

int btn_get_bitmap() {
	return buttons_pressed;
}

void btn_init() {
    adc1_config_width(ADC_WIDTH_BIT_DEFAULT);
    adc1_config_channel_atten(ADC1_CHANNEL_1, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(ADC1_CHANNEL_2, ADC_ATTEN_DB_11);
	esp_timer_handle_t timer;
	const esp_timer_create_args_t conf={
		.callback=btn_cb,
		.name="btn",
		.skip_unhandled_events=true
	};
	esp_timer_create(&conf, &timer);
	esp_timer_start_periodic(timer, SAMPINTER_US);
}