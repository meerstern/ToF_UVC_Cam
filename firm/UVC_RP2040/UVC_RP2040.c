/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pico/stdlib.h>
#include <pico/rand.h>
#include <pico/multicore.h>

#include "bsp/board_api.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "ugui.h"
#include "UVC_RP2040.h"
#include "tmf8829.h"
#include "tmf8829_app.h"
#include "tmf8829_image.h"

UG_GUI gui;

static uint16_t guiBuff[FRAME_WIDTH*FRAME_HEIGHT];//RGB565

volatile uint8_t  tofBuff[TOF_PIX_WIDTH*TOF_PIX_HEIGHT*TOF_PIX_BYTES];
volatile uint16_t tofBuffIndex;
volatile bool     hasTofData = false;
volatile uint8_t  tofIndex;           //Index for interlace mode
static   bool     isTofReady = false;
         char     maxMinStr[40] = {'\0'};

#ifndef CFG_EXAMPLE_VIDEO_READONLY
// YUY2 frame buffer
static uint8_t frame_buffer[FRAME_WIDTH * FRAME_HEIGHT * 16 / 8];
#endif

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task(void* param);
void usb_device_task(void *param);
void video_task(void* param);


void pset(UG_S16 x, UG_S16 y, UG_COLOR col)
{
  uint16_t pix = 1*(x + y*FRAME_WIDTH);  
  guiBuff[pix] = col;
}

void copyFrameBuff()
{
  uint32_t pixels = FRAME_WIDTH * FRAME_HEIGHT;
  uint8_t *dst = &frame_buffer[0];

  for(uint32_t i = 0; i < pixels; i += 2)
  {
    uint16_t p0 = guiBuff[i];
    uint16_t p1 = guiBuff[i + 1];
     
    int r0 = (p0 >> 11) << 3;
    int g0 = ((p0 >> 5) & 0x3F) << 2;
    int b0 = (p0 & 0x1F) << 3;

    int r1 = (p1 >> 11) << 3;
    int g1 = ((p1 >> 5) & 0x3F) << 2;
    int b1 = (p1 & 0x1F) << 3;

    
    uint8_t y0 = (77 * r0 + 150 * g0 + 29 * b0) >> 8;
    uint8_t y1 = (77 * r1 + 150 * g1 + 29 * b1) >> 8;

    int u0 = ((-43 * r0 - 85 * g0 + 128 * b0) >> 8) + 128;
    int v0 = ((128 * r0 -107 * g0 - 21 * b0) >> 8) + 128;

    int u1 = ((-43 * r1 - 85 * g1 + 128 * b1) >> 8) + 128;
    int v1 = ((128 * r1 -107 * g1 - 21 * b1) >> 8) + 128;

    uint8_t u = (u0 + u1) >> 1;
    uint8_t v = (v0 + v1) >> 1;

    *dst++ = y0;
    *dst++ = u;
    *dst++ = y1;
    *dst++ = v;
  }
}

uint16_t val2rgb565( uint16_t value, uint16_t min, uint16_t max)
{
    if(value < min) value = min;
    if(value > max) value = max;

    uint32_t v = 255-((uint32_t)(value - min) * 255)/(max - min);
    uint8_t r, g, b;

    if(v < 64)
    {
        r = 0;
        g = v * 4;
        b = 255;
    }
    else if(v < 128)
    {
        r = 0;
        g = 255;
        b = 255 - (v - 64) * 4;
    }
    else if(v < 192)
    {
        r = (v - 128) * 4;
        g = 255;
        b = 0;
    }
    else
    {
        r = 255;
        g = 255 - (v - 192) * 4;
        b = 0;
    }

    return 
        ((r & 0xF8) << 8) |
        ((g & 0xFC) << 3) |
        (b >> 3);
}

void makeInitGrid()
{
  //return;
  uint16_t x,y;
  for(y=0;y<TOF_PIX_HEIGHT-1;y++)
  {
    for(x=0;x<TOF_PIX_WIDTH-1;x++)
    {
      uint16_t color = get_rand_32();
      uint16_t x_pos = START_X_POS + CELL_SIZE*x;
      uint16_t y_pos = START_Y_POS + CELL_SIZE*y;
      UG_FillFrame(x_pos, y_pos, x_pos+CELL_SIZE, y_pos+CELL_SIZE, color); 

    }
  }
}

void updateGrid()
{
    if(hasTofData)
    {

      printf("\r\nIndex:%d \r\n",tofBuffIndex);

      uint16_t x,y;
      uint16_t dataSize = tofBuffIndex - TMF8829_FRAME_FOOTER_SIZE;
      static uint16_t max_distance = 0;
      static uint16_t min_distance = 5700;

      if(tofIndex==0)
      {
        max_distance = 0;
        min_distance = 5700;
      }

      //Search Max Min Value
      for(uint16_t idx = 0; idx < dataSize/3; idx++)
      {
          uint16_t distance = tofBuff[3*idx+0] + tofBuff[3*idx+1]*256;
          uint16_t confidence =  tofBuff[3*idx+2];
          
          if(confidence>10)
          {
            if(distance>max_distance)
              max_distance = distance;

            if(distance<min_distance)
              min_distance = distance;
          }
      }

      //Update Info
      if(tofIndex==0)
      {         
          uint16_t maxVal = max_distance*TMF8829_BIT2MM/10;//mm to cm 
          memset(maxMinStr, '\0', sizeof(maxMinStr));
          snprintf(maxMinStr,sizeof(maxMinStr),"max= %03d cm    ",maxVal);
          UG_SetForecolor(C_RED);
          UG_PutString(24,77,maxMinStr);

          uint16_t minVal = min_distance*TMF8829_BIT2MM/10;//mm to cm
          memset(maxMinStr, '\0', sizeof(maxMinStr));
          snprintf(maxMinStr,sizeof(maxMinStr),"min= %03d cm    ",minVal);
          UG_SetForecolor(C_BLUE);
          UG_PutString(24,86,maxMinStr);
      }

      if(gpio_get(SW_MODE_ABS)==0)
      {
        if(gpio_get(SW_MODE_SEN)==0)
        {
          max_distance = HIGH_ACC_MAX_RANGE/TMF8829_BIT2MM;
          min_distance = 0;
        }
        else
        {
          max_distance = NORMAL_MAX_RANGE/TMF8829_BIT2MM;
          min_distance = 0;
        }
      }

      //Update Grid Color
      for(uint16_t idx = 0; idx < dataSize/3; idx++)
      {
        uint16_t distance = tofBuff[3*idx+0] + tofBuff[3*idx+1]*256;
        uint16_t confidence =  tofBuff[3*idx+2];
        uint16_t color = val2rgb565(distance, min_distance, max_distance);
        
        uint16_t x = idx%TOF_PIX_WIDTH;
        uint16_t y = 2*(idx/TOF_PIX_WIDTH) + tofIndex;  //Data is interlace
        uint16_t x_pos = START_X_POS + CELL_SIZE*x;
        uint16_t y_pos = START_Y_POS + CELL_SIZE*y;

        static  uint16_t distance_buf[4];
        static  uint16_t confidence_buf[4];
        
        if( x > 27 && x <32 ) //Some pix has bug, Skip Update in tofIndex == 0
        {
            if(y==2)
            {
              distance = distance_buf[x-28];
              confidence = confidence_buf[x-28];
              color = val2rgb565(distance, min_distance, max_distance);
            }
            else if(y==3)
            {
              distance_buf[x-28] = distance;
              confidence_buf[x-28] = confidence;
            }
        }
  
          
        if(confidence>10)
          UG_FillFrame(x_pos, y_pos, x_pos+CELL_SIZE, y_pos+CELL_SIZE, color); 
        else
          UG_FillFrame(x_pos, y_pos, x_pos+CELL_SIZE, y_pos+CELL_SIZE, C_BLACK); 
          
      }

    }
}

void core1_main()
{
Restart:
  setupFn( TMF8829_LOG_LEVEL_ERROR, UART_BAUD_RATE, I2C_CLK_SPEED );//TMF8829_LOG_LEVEL_INFO//TMF8829_LOG_LEVEL_RESULTS//TMF8829_LOG_LEVEL_RESULTS_HEADER//TMF8829_LOG_LEVEL_ERROR
  sleep_ms(10);
  enable( tmf8829_image_start, tmf8829_image, tmf8829_image_length );
  sleep_ms(10);

  if(gpio_get(SW_MODE_SEN)==0)
  {
    preconfigure(TMF8829_CMD_STAT__cmd_stat__CMD_LOAD_CFG_48X32_HIGH_ACCURACY);
  }
  else
  {
    preconfigure(TMF8829_CMD_STAT__cmd_stat__CMD_LOAD_CFG_48X32);
  }
  
  sleep_ms(10);
  measure( );
  isTofReady = true;
  while (1) {
    int8_t res = loopFn();
    if(hasTofData)
    {
      gpio_put(LED_ACT, 1);
      gpio_put(LED_ERR, 0);
      updateGrid();
      tofBuffIndex = 0;
      hasTofData = false;  
    }

    if(res==0)//has Err 
    {
      gpio_put(LED_ACT, 0);
      gpio_put(LED_ERR, 1);
      goto Restart;
    }  
  }

}

//--------------------------------------------------------------------+
// Main
//--------------------------------------------------------------------+
int main(void) {
  board_init();
  stdio_init_all();

  gpio_init(LED_ACT);
  gpio_init(LED_ERR);
  gpio_init(SW_MODE_ABS);
  gpio_init(SW_MODE_SEN);

  gpio_set_dir(LED_ACT, GPIO_OUT);
  gpio_set_dir(LED_ERR, GPIO_OUT);
  gpio_set_dir(SW_MODE_ABS, GPIO_IN);
  gpio_set_dir(SW_MODE_SEN, GPIO_IN);

  gpio_put(LED_ACT, 1);
  gpio_put(LED_ERR, 1);

  uint32_t random_val = get_rand_32();

  memset(guiBuff, 0, sizeof(guiBuff));
  UG_Init(&gui, pset, FRAME_WIDTH, FRAME_HEIGHT);
  UG_FillScreen(C_WHITE);//C_BLUE);//C_WHITE
  UG_SelectGUI(&gui);
  UG_FontSelect(&FONT_8X12);
  UG_SetForecolor(C_BLACK);
  UG_SetBackcolor(C_WHITE);

  char head[] = "ToF CAM v1.1";
  
  UG_PutString(12,1,head);

  UG_FontSelect(&FONT_6X10);
  makeInitGrid();

  // init device stack on configured roothub port
  tusb_rhport_init_t dev_init = {
    .role = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_AUTO
  };
  tusb_init(BOARD_TUD_RHPORT, &dev_init);

  if (board_init_after_tusb) {
    board_init_after_tusb();
  }

  //TMF8829 Process Thread
  multicore_reset_core1();
  multicore_launch_core1(core1_main);

  gpio_put(LED_ACT, 0);
  gpio_put(LED_ERR, 0);

  while (1) {
    tud_task(); // tinyusb device task
    led_blinking_task(NULL);
    video_task(NULL);
  }

}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void) {
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void) {
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en) {
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void) {
  blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}

//--------------------------------------------------------------------+
// USB Video
//--------------------------------------------------------------------+
static unsigned frame_num = 0;
static unsigned tx_busy = 0;
static unsigned interval_ms = 1000 / FRAME_RATE;

#ifdef CFG_EXAMPLE_VIDEO_READONLY
// For mcus that does not have enough SRAM for frame buffer, we use fixed frame data.
// To further reduce the size, we use MJPEG format instead of YUY2.
#include "images.h"

#if !defined(CFG_EXAMPLE_VIDEO_DISABLE_MJPEG)
static struct {
  uint32_t       size;
  uint8_t const *buffer;
} const frames[] = {
  {color_bar_0_jpg_len, color_bar_0_jpg},
  {color_bar_1_jpg_len, color_bar_1_jpg},
  {color_bar_2_jpg_len, color_bar_2_jpg},
  {color_bar_3_jpg_len, color_bar_3_jpg},
  {color_bar_4_jpg_len, color_bar_4_jpg},
  {color_bar_5_jpg_len, color_bar_5_jpg},
  {color_bar_6_jpg_len, color_bar_6_jpg},
  {color_bar_7_jpg_len, color_bar_7_jpg},
};
#endif

#else

// YUY2 frame buffer
//static uint8_t frame_buffer[FRAME_WIDTH * FRAME_HEIGHT * 16 / 8];

static void fill_color_bar(uint8_t* buffer, unsigned start_position) {
  /* EBU color bars: https://stackoverflow.com/questions/6939422 */
  static uint8_t const bar_color[8][4] = {
      /*  Y,   U,   Y,   V */
      { 235, 128, 235, 128}, /* 100% White */
      { 219,  16, 219, 138}, /* Yellow */
      { 188, 154, 188,  16}, /* Cyan */
      { 173,  42, 173,  26}, /* Green */
      {  78, 214,  78, 230}, /* Magenta */
      {  63, 102,  63, 240}, /* Red */
      {  32, 240,  32, 118}, /* Blue */
      {  16, 128,  16, 128}, /* Black */
  };
  uint8_t* p;

  /* Generate the 1st line */
  uint8_t* end = &buffer[FRAME_WIDTH * 2];
  unsigned idx = (FRAME_WIDTH / 2 - 1) - (start_position % (FRAME_WIDTH / 2));
  p = &buffer[idx * 4];
  for (unsigned i = 0; i < 8; ++i) {
    for (int j = 0; j < FRAME_WIDTH / (2 * 8); ++j) {
      memcpy(p, &bar_color[i], 4);
      p += 4;
      if (end <= p) {
        p = buffer;
      }
    }
  }

  /* Duplicate the 1st line to the others */
  p = &buffer[FRAME_WIDTH * 2];
  for (unsigned i = 1; i < FRAME_HEIGHT; ++i) {
    memcpy(p, buffer, FRAME_WIDTH * 2);
    p += FRAME_WIDTH * 2;
  }
}

#endif

void video_send_frame(void) {
  static unsigned start_ms = 0;
  static unsigned already_sent = 0;

  if (!tud_video_n_streaming(0, 0)) {
    already_sent = 0;
    frame_num = 0;
    return;
  }

  if (!already_sent) {
    already_sent = 1;
    tx_busy = 1;
    start_ms = board_millis();
#ifdef CFG_EXAMPLE_VIDEO_READONLY
    #if defined(CFG_EXAMPLE_VIDEO_DISABLE_MJPEG)
    tud_video_n_frame_xfer(0, 0, (void*)(uintptr_t)&frame_buffer[(frame_num % (FRAME_WIDTH / 2)) * 4],
                           FRAME_WIDTH * FRAME_HEIGHT * 16/8);
    #else
    tud_video_n_frame_xfer(0, 0, (void*)(uintptr_t)frames[frame_num % 8].buffer, frames[frame_num % 8].size);
    #endif
#else
    //fill_color_bar(frame_buffer, frame_num);
    if(isTofReady==false)
      makeInitGrid();
    copyFrameBuff();
    tud_video_n_frame_xfer(0, 0, (void*) frame_buffer, FRAME_WIDTH * FRAME_HEIGHT * 16 / 8);
#endif
  }

  unsigned cur = board_millis();
  if (cur - start_ms < interval_ms) return; // not enough time
  if (tx_busy) return;
  start_ms += interval_ms;
  tx_busy = 1;

#ifdef CFG_EXAMPLE_VIDEO_READONLY
  #if defined(CFG_EXAMPLE_VIDEO_DISABLE_MJPEG)
  tud_video_n_frame_xfer(0, 0, (void*)(uintptr_t)&frame_buffer[(frame_num % (FRAME_WIDTH / 2)) * 4],
                         FRAME_WIDTH * FRAME_HEIGHT * 16/8);
  #else
  tud_video_n_frame_xfer(0, 0, (void*)(uintptr_t)frames[frame_num % 8].buffer, frames[frame_num % 8].size);
  #endif
#else
  //fill_color_bar(frame_buffer, frame_num);
  if(isTofReady==false)
    makeInitGrid();
  copyFrameBuff();
  tud_video_n_frame_xfer(0, 0, (void*) frame_buffer, FRAME_WIDTH * FRAME_HEIGHT * 16 / 8);
#endif
}


void video_task(void* param) {
  (void) param;

  while(1) {
    video_send_frame();
    return;

  }
}

void tud_video_frame_xfer_complete_cb(uint_fast8_t ctl_idx, uint_fast8_t stm_idx) {
  (void) ctl_idx;
  (void) stm_idx;
  tx_busy = 0;
  /* flip buffer */
  //++frame_num;
}

int tud_video_commit_cb(uint_fast8_t ctl_idx, uint_fast8_t stm_idx,
                        video_probe_and_commit_control_t const* parameters) {
  (void) ctl_idx;
  (void) stm_idx;
  /* convert unit to ms from 100 ns */
  interval_ms = parameters->dwFrameInterval / 10000;
  return VIDEO_ERROR_NONE;
}

//--------------------------------------------------------------------+
// Blinking Task
//--------------------------------------------------------------------+
void led_blinking_task(void* param) {
  (void) param;
  static uint32_t start_ms = 0;
  static bool led_state = false;

  while (1) {

    if (board_millis() - start_ms < blink_interval_ms) return; // not enough time


    start_ms += blink_interval_ms;
    board_led_write(led_state);
    led_state = 1 - led_state; // toggle
  }
}

