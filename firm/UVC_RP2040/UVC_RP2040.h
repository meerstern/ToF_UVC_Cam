#ifndef UVC_RP2040_H
#define UVC_RP2040_H
#include <stdbool.h>

/* UGUI Settings */
#define START_X_POS     14
#define START_Y_POS     12
#define CELL_SIZE       2
#define TOF_PIX_WIDTH   48
#define TOF_PIX_HEIGHT  32
#define TOF_PIX_BYTES   3   //Confidence 1byte, Distance 2byte
#define TMF8829_CONF_TH 30

#define TMF8829_BIT2MM  (0.25f)

//#define MIRRR_OUTPUT_MODE   1
#define TOF_HEADER_VERSION  "ToF CAM v1.2"

/* Communication Settings */

#define F(x) x

// SPI Defines
// We are going to use SPI 0, and allocate it to the following GPIO pins
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define SPI_PORT spi0
#define PIN_MISO 4//16
#define PIN_CS   5//17
#define PIN_SCK  2//18
#define PIN_MOSI 3//19

// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c0
#define I2C_SDA 28//8
#define I2C_SCL 29//9

#define LED_ACT 22
#define LED_ERR 23   

#define SW_MODE_ABS 24
#define SW_MODE_SEN 25
#define SW_ACT      17  //For Future Use

#define I2C_CLK_SPEED               400000
#define UART_BAUD_RATE              115200


#define NORMAL_MAX_RANGE            (5700.0f)//mm
#define HIGH_ACC_MAX_RANGE          (1400.0f)//mm

extern volatile uint8_t     tofBuff[TOF_PIX_WIDTH*TOF_PIX_HEIGHT*TOF_PIX_BYTES];
extern volatile uint16_t    tofBuffIndex;
extern volatile bool        hasTofData;
extern volatile uint8_t     tofIndex;
#endif