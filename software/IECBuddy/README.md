## Compiling the firmware from source

In most cases it is easiest to [simply upload the provided .uf2 files]([../../../README.md#uploading-the-firmware](https://github.com/dhansel/IECBuddy/tree/main?tab=readme-ov-file#uploading-the-firmware)) to the RP2040-One.

If you would like to compile the firmware from source then follow these steps:
  1) Download this GitHub repository.
  2) Load the "IECBuddy.ino" file from this directory in the Arduino IDE.
  3) Select the "Waveshare RP2040-One" board (or Raspberry Pi Pico 1 or 2 for the Max variant).
  4) Edit the "Pins.h" file to configure for your desired variant:
     - For the Barebones and Micro variants no changes are required
     - For the Mini or Max variants, un-comment the #defines for PIN_IEC_CLK_OUT and PIN_IEC_DATA_OUT
  5) If your build should support the ST7789 display then un-comment the PIN_ST7789_* defines. In that case you
    also need to make sure the "Adafruit GFX Library" is installed. You can compile in the display support even
    if you don't actually have a display.
  6) Plug in your RP2040-One or Pi Pico board and click the "upload" button in the Arduino IDE.
