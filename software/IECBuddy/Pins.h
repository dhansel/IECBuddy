#ifndef PINS_H
#define PINS_H

// GPIO 0/1 are UART TX/RX, reserved for debugging
// On Pi Pico,    GPIO 16+22+28 are currently not used (GPIO 23-25,29 not available on board)
// On RP2040-One, GPIO 9-13,28+29 are currently not used (GPIO 16-25 not available on board)

// The ATN/CLK/DATA IEC connections are the only required pins
// (RESET can be commented out if not connected)
#define PIN_IEC_ATN           2
#define PIN_IEC_CLK           3
#define PIN_IEC_DATA          4
#define PIN_IEC_RESET         5

// Un-comment both of these if using 74LS06 line driver for CLK/DATA line output
//#define PIN_IEC_CLK_OUT       6
//#define PIN_IEC_DATA_OUT      7

// "Disk Change" button
#define PIN_BUTTON            8

// Parallel cable (for DolphinDos) is only supported on Pi Pico boards (more pins)
// (commenting out these #defines will remove DolphinDos support on the Pico too)
#if defined(ARDUINO_RASPBERRY_PI_PICO) || defined(ARDUINO_RASPBERRY_PI_PICO_2)
#define PIN_PAR_FLAG2         9
#define PIN_PAR_PB0          10
#define PIN_PAR_PB1          11
#define PIN_PAR_PB2          12
#define PIN_PAR_PB3          13
#define PIN_PAR_PB4          17
#define PIN_PAR_PB5          18
#define PIN_PAR_PB6          19
#define PIN_PAR_PB7          20
#define PIN_PAR_PC2          21
#endif

// Waveshare RP2040-One board uses an RGB LED on GPIO16
#if defined(ARDUINO_WAVESHARE_RP2040_ONE) || defined(ARDUINO_GENERIC_RP2350)
#define PIN_DRIVE_LED_IS_NEOPIXEL_RGB
#define PIN_DRIVE_LED        16
#elif defined(PIN_LED)
#define PIN_DRIVE_LED        PIN_LED
#endif

// Un-comment PIN_ST7789* to support a ST7789 TFT IPS display
// If enabled, the "GFX Library for Arduino" library must be installed in the Arduino IDE 
// GPIO 0-7 use SPI0
// GPIO8-16 use SPI1
//#define PIN_ST7789_SPI       SPI1
//#define PIN_ST7789_SPI_SCL   14
//#define PIN_ST7789_SPI_COPI  15
//#define PIN_ST7789_RES       26
//#define PIN_ST7789_DC        27

#endif
