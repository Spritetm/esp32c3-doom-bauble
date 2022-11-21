#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#define PIN_NUM_MISO -1
#define PIN_NUM_MOSI 9
#define PIN_NUM_CLK	 8
#define PIN_NUM_CS	 5

#define PIN_NUM_DC	 7
#define PIN_NUM_RST	 6
#define PIN_NUM_BCKL 4

#define PARALLEL_LINES 4

#define ST7735_MADCTL_BGR 0x08
#define ST7735_MADCTL_MH  0x04

#define ST7735_FRMCTR1 0xB1
#define ST7735_FRMCTR2 0xB2
#define ST7735_FRMCTR3 0xB3
#define ST7735_INVCTR  0xB4
#define ST7735_DISSET5 0xB6

#define ST7735_PWCTR1  0xC0
#define ST7735_PWCTR2  0xC1
#define ST7735_PWCTR3  0xC2
#define ST7735_PWCTR4  0xC3
#define ST7735_PWCTR5  0xC4
#define ST7735_VMCTR1  0xC5

#define ST7735_PWCTR6  0xFC

#define ST7735_GMCTRP1 0xE0
#define ST7735_GMCTRN1 0xE1

#define ST_CMD_DELAY   0x80	   // special signifier for command lists

#define ST77XX_NOP	   0x00
#define ST77XX_SWRESET 0x01
#define ST77XX_RDDID   0x04
#define ST77XX_RDDST   0x09

#define ST77XX_SLPIN   0x10
#define ST77XX_SLPOUT  0x11
#define ST77XX_PTLON   0x12
#define ST77XX_NORON   0x13

#define ST77XX_INVOFF  0x20
#define ST77XX_INVON   0x21
#define ST77XX_DISPOFF 0x28
#define ST77XX_DISPON  0x29
#define ST77XX_CASET   0x2A
#define ST77XX_RASET   0x2B
#define ST77XX_RAMWR   0x2C
#define ST77XX_RAMRD   0x2E

#define ST77XX_PTLAR   0x30
#define ST77XX_COLMOD  0x3A
#define ST77XX_MADCTL  0x36

#define ST77XX_MADCTL_MY  0x80
#define ST77XX_MADCTL_MX  0x40
#define ST77XX_MADCTL_MV  0x20
#define ST77XX_MADCTL_ML  0x10
#define ST77XX_MADCTL_RGB 0x00

#define ST77XX_RDID1   0xDA
#define ST77XX_RDID2   0xDB
#define ST77XX_RDID3   0xDC
#define ST77XX_RDID4   0xDD


static const uint8_t
  Bcmd[] = {				  // Initialization commands for 7735B screens
	18,						  // 18 commands in list:
	ST77XX_SWRESET,	  ST_CMD_DELAY,	 //	 1: Software reset, no args, w/delay
	  50,					  //	 50 ms delay
	ST77XX_SLPOUT ,	  ST_CMD_DELAY,	 //	 2: Out of sleep mode, no args, w/delay
	  255,					  //	 255 = 500 ms delay
	ST77XX_COLMOD , 1+ST_CMD_DELAY,	 //	 3: Set color mode, 1 arg + delay:
	  0x05,					  //	 16-bit color
	  10,					  //	 10 ms delay
	ST7735_FRMCTR1, 3+ST_CMD_DELAY,	 //	 4: Frame rate control, 3 args + delay:
	  0x00,					  //	 fastest refresh
	  0x06,					  //	 6 lines front porch
	  0x03,					  //	 3 lines back porch
	  10,					  //	 10 ms delay
	ST77XX_MADCTL , 1	   ,  //  5: Memory access ctrl (directions), 1 arg:
	  0x08,					  //	 Row addr/col addr, bottom to top refresh
	ST7735_DISSET5, 2	   ,  //  6: Display settings #5, 2 args, no delay:
	  0x15,					  //	 1 clk cycle nonoverlap, 2 cycle gate
							  //	 rise, 3 cycle osc equalize
	  0x02,					  //	 Fix on VTL
	ST7735_INVCTR , 1	   ,  //  7: Display inversion control, 1 arg:
	  0x0,					  //	 Line inversion
	ST7735_PWCTR1 , 2+ST_CMD_DELAY,	 //	 8: Power control, 2 args + delay:
	  0x02,					  //	 GVDD = 4.7V
	  0x70,					  //	 1.0uA
	  10,					  //	 10 ms delay
	ST7735_PWCTR2 , 1	   ,  //  9: Power control, 1 arg, no delay:
	  0x05,					  //	 VGH = 14.7V, VGL = -7.35V
	ST7735_PWCTR3 , 2	   ,  // 10: Power control, 2 args, no delay:
	  0x01,					  //	 Opamp current small
	  0x02,					  //	 Boost frequency
	ST7735_VMCTR1 , 2+ST_CMD_DELAY,	 // 11: Power control, 2 args + delay:
	  0x3C,					  //	 VCOMH = 4V
	  0x38,					  //	 VCOML = -1.1V
	  10,					  //	 10 ms delay
	ST7735_PWCTR6 , 2	   ,  // 12: Power control, 2 args, no delay:
	  0x11, 0x15,
	ST7735_GMCTRP1,16	   ,  // 13: Magical unicorn dust, 16 args, no delay:
	  0x09, 0x16, 0x09, 0x20, //	 (seriously though, not sure what
	  0x21, 0x1B, 0x13, 0x19, //	  these config values represent)
	  0x17, 0x15, 0x1E, 0x2B,
	  0x04, 0x05, 0x02, 0x0E,
	ST7735_GMCTRN1,16+ST_CMD_DELAY,	 // 14: Sparkles and rainbows, 16 args + delay:
	  0x0B, 0x14, 0x08, 0x1E, //	 (ditto)
	  0x22, 0x1D, 0x18, 0x1E,
	  0x1B, 0x1A, 0x24, 0x2B,
	  0x06, 0x06, 0x02, 0x0F,
	  10,					  //	 10 ms delay
	ST77XX_CASET  , 4	   ,  // 15: Column addr set, 4 args, no delay:
	  0x00, 0x02,			  //	 XSTART = 2
	  0x00, 0x81,			  //	 XEND = 129
	ST77XX_RASET  , 4	   ,  // 16: Row addr set, 4 args, no delay:
	  0x00, 0x02,			  //	 XSTART = 1
	  0x00, 0x81,			  //	 XEND = 160
	ST77XX_NORON  ,	  ST_CMD_DELAY,	 // 17: Normal display on, no args, w/delay
	  10,					  //	 10 ms delay
	ST77XX_DISPON ,	  ST_CMD_DELAY,	 // 18: Main screen turn on, no args, w/delay
	  255 },				  //	 255 = 500 ms delay

  Rcmd1[] = {				  // Init for 7735R, part 1 (red or green tab)
	15,						  // 15 commands in list:
	ST77XX_SWRESET,	  ST_CMD_DELAY,	 //	 1: Software reset, 0 args, w/delay
	  150,					  //	 150 ms delay
	ST77XX_SLPOUT ,	  ST_CMD_DELAY,	 //	 2: Out of sleep mode, 0 args, w/delay
	  255,					  //	 500 ms delay
	ST7735_FRMCTR1, 3	   ,  //  3: Frame rate ctrl - normal mode, 3 args:
	  0x01, 0x2C, 0x2D,		  //	 Rate = fosc/(1x2+40) * (LINE+2C+2D)
	ST7735_FRMCTR2, 3	   ,  //  4: Frame rate control - idle mode, 3 args:
	  0x01, 0x2C, 0x2D,		  //	 Rate = fosc/(1x2+40) * (LINE+2C+2D)
	ST7735_FRMCTR3, 6	   ,  //  5: Frame rate ctrl - partial mode, 6 args:
	  0x01, 0x2C, 0x2D,		  //	 Dot inversion mode
	  0x01, 0x2C, 0x2D,		  //	 Line inversion mode
	ST7735_INVCTR , 1	   ,  //  6: Display inversion ctrl, 1 arg, no delay:
	  0x07,					  //	 No inversion
	ST7735_PWCTR1 , 3	   ,  //  7: Power control, 3 args, no delay:
	  0xA2,
	  0x02,					  //	 -4.6V
	  0x84,					  //	 AUTO mode
	ST7735_PWCTR2 , 1	   ,  //  8: Power control, 1 arg, no delay:
	  0xC5,					  //	 VGH25 = 2.4C VGSEL = -10 VGH = 3 * AVDD
	ST7735_PWCTR3 , 2	   ,  //  9: Power control, 2 args, no delay:
	  0x0A,					  //	 Opamp current small
	  0x00,					  //	 Boost frequency
	ST7735_PWCTR4 , 2	   ,  // 10: Power control, 2 args, no delay:
	  0x8A,					  //	 BCLK/2, Opamp current small & Medium low
	  0x2A,	 
	ST7735_PWCTR5 , 2	   ,  // 11: Power control, 2 args, no delay:
	  0x8A, 0xEE,
	ST7735_VMCTR1 , 1	   ,  // 12: Power control, 1 arg, no delay:
	  0x0E,
	ST77XX_INVON , 0	   ,  // 13: Don't invert display, no args, no delay  (Note: display shows inverted - re-invert?)
	ST77XX_MADCTL , 1	   ,  // 14: Memory access control (directions), 1 arg:
	  0xC8,					  //	 row addr/col addr, bottom to top refresh
	ST77XX_COLMOD , 1	   ,  // 15: set color mode, 1 arg, no delay:
	  0x05 },				  //	 16-bit color
  Rcmd2green160x80[] = {			  // Init for 7735R, part 2 (mini 160x80)
	2,						  //  2 commands in list:
	ST77XX_CASET  , 4	   ,  //  1: Column addr set, 4 args, no delay:
	  0x00, 0x00,			  //	 XSTART = 0
	  0x00, 0x7F,			  //	 XEND = 79
	ST77XX_RASET  , 4	   ,  //  2: Row addr set, 4 args, no delay:
	  0x00, 0x00,			  //	 XSTART = 0
	  0x00, 0x9F },			  //	 XEND = 159


  Rcmd3[] = {				  // Init for 7735R, part 3 (red or green tab)
	4,						  //  4 commands in list:
	ST7735_GMCTRP1, 16		, //  1: Magical unicorn dust, 16 args, no delay:
	  0x02, 0x1c, 0x07, 0x12,
	  0x37, 0x32, 0x29, 0x2d,
	  0x29, 0x25, 0x2B, 0x39,
	  0x00, 0x01, 0x03, 0x10,
	ST7735_GMCTRN1, 16		, //  2: Sparkles and rainbows, 16 args, no delay:
	  0x03, 0x1d, 0x07, 0x06,
	  0x2E, 0x2C, 0x29, 0x2D,
	  0x2E, 0x2E, 0x37, 0x3F,
	  0x00, 0x00, 0x02, 0x10,
	ST77XX_NORON  ,	   ST_CMD_DELAY, //	 3: Normal display on, no args, w/delay
	  10,					  //	 10 ms delay
	ST77XX_DISPON ,	   ST_CMD_DELAY, //	 4: Main screen turn on, no args w/delay
	  100 };				  //	 100 ms delay


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

static void send_cmd_list(spi_device_handle_t spi, const uint8_t *cmd) {
	int no_cmd=*cmd++;
	uint8_t buf[32];
	for (int cmdno=0; cmdno<no_cmd; cmdno++) {
		int mycmd=*cmd++;
		lcd_cmd(spi, mycmd);
		int flag=*cmd++;
		printf("Cmd %x delay param %x\n", mycmd, flag);
		if (flag&31) {
			memcpy(buf, cmd, flag&31);
			cmd+=(flag&31);
			lcd_data(spi,buf, (flag&31));
		}
		if (flag&ST_CMD_DELAY) {
			vTaskDelay(pdMS_TO_TICKS(*cmd++));
		}
	}
}


//Initialize the display
void lcd_init_controller(spi_device_handle_t spi) {

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
	vTaskDelay(pdMS_TO_TICKS(100));
	gpio_set_level(PIN_NUM_RST, 1);
	vTaskDelay(pdMS_TO_TICKS(100));

	send_cmd_list(spi, Bcmd);
	send_cmd_list(spi, Rcmd1);
	send_cmd_list(spi, Rcmd2green160x80);
	send_cmd_list(spi, Rcmd3);

	///Enable backlight
	gpio_set_level(PIN_NUM_BCKL, 1);	//For some screens, you may need to set 1 to light its backlight
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
	int ox=26;
	int oy=1;
	ypos+=oy;
	trans[0].tx_data[0]=ST77XX_CASET;			//Column Address Set
	trans[1].tx_data[0]=ox;				//Start Col High
	trans[1].tx_data[1]=ox;				//Start Col Low
	trans[1].tx_data[2]=(79+ox)>>8;		//End Col High
	trans[1].tx_data[3]=(79+ox)&0xff;		//End Col Low
	trans[2].tx_data[0]=ST77XX_RASET;			//Page address set
	trans[3].tx_data[0]=ypos>>8;		//Start page high
	trans[3].tx_data[1]=ypos&0xff;		//start page low
	trans[3].tx_data[2]=(ypos+PARALLEL_LINES)>>8;	 //end page high
	trans[3].tx_data[3]=(ypos+PARALLEL_LINES)&0xff;	 //end page low
	trans[4].tx_data[0]=ST77XX_RAMWR;			//memory write
	trans[5].tx_buffer=linedata;		//finally send the line data
	trans[5].length=80*2*8*PARALLEL_LINES;			 //Data length, in bits
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


static SemaphoreHandle_t sem_start, sem_done;

static uint16_t lcdpal[256];
static uint8_t *cur_fb;


static uint16_t *line_a, *line_b, *line;
static spi_device_handle_t spi;


//Take the r8, g8, b8 palette passed in and convert to an uint16_t we can send to the LCD.
//Store this locally in lcdpal.
void lcd_set_pal(uint8_t *pal) {
	for (int i=0; i<256; i++) {
		uint8_t r=*pal++;
		uint8_t g=*pal++;
		uint8_t b=*pal++;
		uint16_t pp=((r>>3)<<11)|((g>>2)<<5)|((b>>3));
		lcdpal[i]=(pp>>8)|(pp<<8);
	}
}


void lcd_render_fb(uint8_t *fb) {
	line=line_a;

	//This scales the originally 160x240 image to the 80x60 size we need.
	int y_sz=60;

	for (int yy=0; yy<160; yy+=PARALLEL_LINES) {
		uint16_t *p=line;
		for (int y=0; y<PARALLEL_LINES; y++) {
			int ef_y=((160*(yy+y))/y_sz)-28;
			uint8_t *fbline=&fb[ef_y*240];
			if (ef_y>=160 || ef_y<0) {
				for (int x=0; x<80; x++) *p++=0;
			} else {
				for (int x=0; x<80; x++) {
					*p++=lcdpal[*fbline++];
					//We scale 240 -> 80, so increase fb by 3
					fbline+=2;
				}
			}
		}
		send_line_finish(spi);
//		send_lines(spi, yy, line);
		send_lines(spi, yy+80, line); //HACK
		if (line==line_a) line=line_b; else line=line_a; //flip
	}
}

#define LCD_HOST SPI2_HOST

void lcd_init() {
	esp_err_t ret;
	spi_bus_config_t buscfg={
		.miso_io_num=PIN_NUM_MISO,
		.mosi_io_num=PIN_NUM_MOSI,
		.sclk_io_num=PIN_NUM_CLK,
		.quadwp_io_num=-1,
		.quadhd_io_num=-1,
		.max_transfer_sz=PARALLEL_LINES*320*2+8
	};
	spi_device_interface_config_t devcfg={
		.clock_speed_hz=26*1000*1000,			//Clock out at 26 MHz
		.mode=0,								//SPI mode 0
		.spics_io_num=PIN_NUM_CS,				//CS pin
		.queue_size=7,							//We want to be able to queue 7 transactions at a time
		.pre_cb=lcd_spi_pre_transfer_callback,	//Specify pre-transfer callback to handle D/C line
	};
	//Initialize the SPI bus
	ret=spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
	ESP_ERROR_CHECK(ret);
	//Attach the LCD to the SPI bus
	ret=spi_bus_add_device(LCD_HOST, &devcfg, &spi);
	ESP_ERROR_CHECK(ret);
	//Initialize the LCD
	lcd_init_controller(spi);

	//allocate line buffers
	line_a=calloc(PARALLEL_LINES, 80*2);
	line_b=calloc(PARALLEL_LINES, 80*2);
	assert(line_a);
	assert(line_b);

	//clear entire screen
	//also leave one transaction in the spi buffer, as the code above assumes there
	//is one.
	send_lines(spi, 0, line_a);
	for (int y=PARALLEL_LINES; y<80; y+=PARALLEL_LINES) {
		send_line_finish(spi);
		send_lines(spi, y, line_a);
	}
}


