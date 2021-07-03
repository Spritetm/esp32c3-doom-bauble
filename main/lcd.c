#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#define PIN_NUM_MISO 9
#define PIN_NUM_MOSI 5
#define PIN_NUM_CLK	 6
#define PIN_NUM_CS	 7

#define PIN_NUM_DC	 4
#define PIN_NUM_RST	 10
#define PIN_NUM_BCKL 8

/*
 The LCD needs a bunch of command/argument values to be initialized. They are stored in this struct.
*/
typedef struct {
	uint8_t cmd;
	uint8_t data[16];
	uint8_t databytes; //No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} lcd_init_cmd_t;

typedef enum {
	LCD_TYPE_ILI = 1,
	LCD_TYPE_ST,
	LCD_TYPE_MAX,
} type_lcd_t;

//Place data into DRAM. Constant data gets placed into DROM by default, which is not accessible by DMA.
DRAM_ATTR static const lcd_init_cmd_t init_cmds[]={
	/* Memory Data Access Control, MX=MV=1, MY=ML=MH=0, RGB=0 */
	{0x36, {(1<<5)|(1<<6)}, 1},
	/* Interface Pixel Format, 16bits/pixel for RGB/MCU interface */
	{0x3A, {0x55}, 1},
	/* Porch Setting */
	{0xB2, {0x0c, 0x0c, 0x00, 0x33, 0x33}, 5},
	/* Gate Control, Vgh=13.65V, Vgl=-10.43V */
	{0xB7, {0x45}, 1},
	/* VCOM Setting, VCOM=1.175V */
	{0xBB, {0x2B}, 1},
	/* LCM Control, XOR: BGR, MX, MH */
	{0xC0, {0x2C}, 1},
	/* VDV and VRH Command Enable, enable=1 */
	{0xC2, {0x01, 0xff}, 2},
	/* VRH Set, Vap=4.4+... */
	{0xC3, {0x11}, 1},
	/* VDV Set, VDV=0 */
	{0xC4, {0x20}, 1},
	/* Frame Rate Control, 60Hz, inversion=0 */
	{0xC6, {0x0f}, 1},
	/* Power Control 1, AVDD=6.8V, AVCL=-4.8V, VDDS=2.3V */
	{0xD0, {0xA4, 0xA1}, 1},
	/* Positive Voltage Gamma Control */
	{0xE0, {0xD0, 0x00, 0x05, 0x0E, 0x15, 0x0D, 0x37, 0x43, 0x47, 0x09, 0x15, 0x12, 0x16, 0x19}, 14},
	/* Negative Voltage Gamma Control */
	{0xE1, {0xD0, 0x00, 0x05, 0x0D, 0x0C, 0x06, 0x2D, 0x44, 0x40, 0x0E, 0x1C, 0x18, 0x16, 0x19}, 14},
	/* Sleep Out */
	{0x11, {0}, 0x80},
	/* Display On */
	{0x29, {0}, 0x80},
	{0, {0}, 0xff}
};

/* Send a command to the LCD. Uses spi_device_polling_transmit, which waits
 * until the transfer is complete.
 */
void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd)
{
	esp_err_t ret;
	spi_transaction_t t;
	memset(&t, 0, sizeof(t));		//Zero out the transaction
	t.length=8;						//Command is 8 bits
	t.tx_buffer=&cmd;				//The data is the cmd itself
	t.user=(void*)0;				//D/C needs to be set to 0
	ret=spi_device_polling_transmit(spi, &t);  //Transmit!
	assert(ret==ESP_OK);			//Should have had no issues.
}

/* Send data to the LCD. Uses spi_device_polling_transmit, which waits until the
 * transfer is complete.
 */
void lcd_data(spi_device_handle_t spi, const uint8_t *data, int len) {
	esp_err_t ret;
	spi_transaction_t t;
	if (len==0) return;				//no need to send anything
	memset(&t, 0, sizeof(t));		//Zero out the transaction
	t.length=len*8;					//Len is in bytes, transaction length is in bits.
	t.tx_buffer=data;				//Data
	t.user=(void*)1;				//D/C needs to be set to 1
	ret=spi_device_polling_transmit(spi, &t);  //Transmit!
	assert(ret==ESP_OK);			//Should have had no issues.
}

//This function is called (in irq context!) just before a transmission starts. It will
//set the D/C line to the value indicated in the user field.
void lcd_spi_pre_transfer_callback(spi_transaction_t *t) {
	int dc=(int)t->user;
	gpio_set_level(PIN_NUM_DC, dc);
}

//Initialize the display
void lcd_init(spi_device_handle_t spi) {
	int cmd=0;
	const lcd_init_cmd_t* lcd_init_cmds=init_cmds;

	//Initialize non-SPI GPIOs
	gpio_config_t cfg = {
		.pin_bit_mask = BIT64(PIN_NUM_DC) | BIT64(PIN_NUM_RST) | BIT64(PIN_NUM_BCKL),
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = false,
		.pull_down_en = false,
		.intr_type = GPIO_INTR_DISABLE,
	};
	gpio_config(&cfg);

	//Reset the display
	gpio_set_level(PIN_NUM_RST, 0);
	vTaskDelay(100 / portTICK_RATE_MS);
	gpio_set_level(PIN_NUM_RST, 1);
	vTaskDelay(100 / portTICK_RATE_MS);

	//Send all the commands
	while (lcd_init_cmds[cmd].databytes!=0xff) {
		lcd_cmd(spi, lcd_init_cmds[cmd].cmd);
		lcd_data(spi, lcd_init_cmds[cmd].data, lcd_init_cmds[cmd].databytes&0x1F);
		if (lcd_init_cmds[cmd].databytes&0x80) {
			vTaskDelay(100 / portTICK_RATE_MS);
		}
		cmd++;
	}

	///Enable backlight
	gpio_set_level(PIN_NUM_BCKL, 0);	//For some screens, you may need to set 1 to light its backlight
}


static void send_lines(spi_device_handle_t spi, int ypos, uint16_t *linedata) {
	esp_err_t ret;
	int x;
	//Transaction descriptors. Declared static so they're not allocated on the stack; we need this memory even when this
	//function is finished because the SPI driver needs access to it even while we're already calculating the next line.
	static spi_transaction_t trans[6];

	//In theory, it's better to initialize trans and data only once and hang on to the initialized
	//variables. We allocate them on the stack, so we need to re-init them each call.
	for (x=0; x<6; x++) {
		memset(&trans[x], 0, sizeof(spi_transaction_t));
		if ((x&1)==0) {
			//Even transfers are commands
			trans[x].length=8;
			trans[x].user=(void*)0;
		} else {
			//Odd transfers are data
			trans[x].length=8*4;
			trans[x].user=(void*)1;
		}
		trans[x].flags=SPI_TRANS_USE_TXDATA;
	}
	trans[0].tx_data[0]=0x2A;			//Column Address Set
	trans[1].tx_data[0]=0;				//Start Col High
	trans[1].tx_data[1]=0;				//Start Col Low
	trans[1].tx_data[2]=(320)>>8;		//End Col High
	trans[1].tx_data[3]=(320)&0xff;		//End Col Low
	trans[2].tx_data[0]=0x2B;			//Page address set
	trans[3].tx_data[0]=ypos>>8;		//Start page high
	trans[3].tx_data[1]=ypos&0xff;		//start page low
	trans[3].tx_data[2]=(ypos+PARALLEL_LINES)>>8;	 //end page high
	trans[3].tx_data[3]=(ypos+PARALLEL_LINES)&0xff;	 //end page low
	trans[4].tx_data[0]=0x2C;			//memory write
	trans[5].tx_buffer=linedata;		//finally send the line data
	trans[5].length=320*2*8*PARALLEL_LINES;			 //Data length, in bits
	trans[5].flags=0; //undo SPI_TRANS_USE_TXDATA flag

	//Queue all transactions.
	for (x=0; x<6; x++) {
		ret=spi_device_queue_trans(spi, &trans[x], portMAX_DELAY);
		assert(ret==ESP_OK);
	}

	//When we are here, the SPI driver is busy (in the background) getting the transactions sent. That happens
	//mostly using DMA, so the CPU doesn't have much to do here. We're not going to wait for the transaction to
	//finish because we may as well spend the time calculating the next line. When that is done, we can call
	//send_line_finish, which will wait for the transfers to be done and check their status.
}

static void send_line_finish(spi_device_handle_t spi) {
	spi_transaction_t *rtrans;
	esp_err_t ret;
	//Wait for all 6 transactions to be done and get back the results.
	for (int x=0; x<6; x++) {
		ret=spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
		assert(ret==ESP_OK);
		//We could inspect rtrans now if we received any info back. The LCD is treated as write-only, though.
	}
}
