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
#include "driver/i2s_pdm.h"
#include "soc/gpio_sig_map.h"
#include "esp_log.h"

const char *TAG="sndc3";

#define BUFSZ 128

static snd_cb_t *snd_cb;
i2s_chan_handle_t i2s_handle;

void snd_task(void *arg) {
	int16_t buf[BUFSZ];
	while(1) {
		snd_cb(buf, BUFSZ);
		size_t written=0;
		ESP_ERROR_CHECK(i2s_channel_write(i2s_handle, buf, BUFSZ*2, &written, 1000));
	}
}

void snd_init(int samprate, snd_cb_t *cb) {
	snd_cb=cb;

	i2s_chan_config_t i2s_config={
		.id=0,
		.role=I2S_ROLE_MASTER,
		.dma_desc_num=2,
		.dma_frame_num=128,
		.auto_clear=true
	};
	ESP_ERROR_CHECK(i2s_new_channel(&i2s_config, &i2s_handle, NULL));
	
	i2s_pdm_tx_config_t i2s_pdm_config={
		.clk_cfg=I2S_PDM_TX_CLK_DEFAULT_CONFIG(samprate),
		.slot_cfg=I2S_PDM_TX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
		.gpio_cfg={
			.clk=GPIO_NUM_NC,
			.dout=0,
		}
	};
	
	ESP_ERROR_CHECK(i2s_channel_init_pdm_tx_mode(i2s_handle, &i2s_pdm_config));
	i2s_channel_enable(i2s_handle);

	//Start task to handle sound stuff
	xTaskCreate(snd_task, "snd", 2*1024, NULL, 5, NULL);

	vTaskDelay(pdMS_TO_TICKS(200));

	//GPIO1 is the inverse of GPIO0
	esp_rom_gpio_connect_out_signal(1, I2SO_SD_OUT_IDX, 1, 0);
}

