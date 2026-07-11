# ToF UVC Camera Board

## Overview 
  * This is a ToF camera board equipped with the [AMS TMF8829 ToF sensor][1].  
  * Outputs 48x32 ToF data as a UVC webcam image.  
  * The microcontroller used is the RP2040.  
  * Source code using the Pico-SDK is available.
  * Measuring up to 1.4 m in high-resolution mode and up to 5.7 m in standard mode.


## Detailed Specifications
  * The USB port uses a Type-C connection.
  * The board is recognized as a UVC (USB Video Class) device with a resolution of 128x96.
  * The ToF resolution is 48x32 as a UVC webcam image.
  * Measuring up to 1.4 m in high-resolution mode(SW3 SEN ON)
  * Measuring up to 5.7 m in standard mode(SW3 SEN OFF)
  * Switch the color scale between absolute(SW3 ABS ON) and relative modes(SW3 ABS OFF).
  * In the relative mode, the color scale is calculated based on the overall maximum and minimum values.
  * For absolute mode, the color scale is calculated based on a minimum of 0 and a maximum of 1.4 m in high-resolution mode, or 5.7 m in normal mode.
  * The frame rate depends on the conditions, but it is around 8 FPS.
  * 80° FOV, 1cm minimum distance and 0.25 mm resolution 
  * The maximum range of 11m is supported only at 8x8 resolution; this board does not support it.
  * The board size is 40mm x50 mm, the hole size is M3x4, the hole pitch is 34mm x44mm. 
  
## Updating the firmware
  * Connect the device to a PC (or similar) via USB Type-C.
  * Then press the RST button while holding down the BOOT button.
  * The device will be recognized as a USB drive.  
  * You can update the firmware by copying the firmware file(*.uf2) to the recognized USB drive.

## Appearance and examples
<img src="img/img1.jpeg" width="360">
<img src="img/img2.jpeg" width="360">
<img src="img/img3.gif" width="360">

[1]: https://ams-osram.com/ja/products/sensor-solutions/direct-time-of-flight-sensors-dtof/ams-tmf8829-48x32-multi-zone-time-of-flight-sensor
