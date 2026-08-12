# TFT_eSPI User_Setup.h Setup Guide for 4ZE Dashboard v6.0

To prevent SPI bus conflicts between the TFT Display and MCP2515 CAN module, configure `TFT_eSPI` library's `User_Setup.h` file with these exact lines:

## 1. File Location
Open `Arduino/libraries/TFT_eSPI/User_Setup.h` in your text editor or Arduino IDE.

## 2. Required Configurations

```cpp
// ------------------------------------------------------------------
// Driver Selection
// ------------------------------------------------------------------
#define ILI9488_DRIVER

// ------------------------------------------------------------------
// SPI Bus Assignment (CRITICAL)
// ------------------------------------------------------------------
// Tells TFT_eSPI to use HSPI so MCP2515 CAN can use VSPI without conflicts
#define USE_HSPI_PORT

// ------------------------------------------------------------------
// Pin Assignments (ESP32 DevKit V1)
// ------------------------------------------------------------------
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC   27
#define TFT_RST  33

// ------------------------------------------------------------------
// Fonts
// ------------------------------------------------------------------
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT7
#define LOAD_FONT8    // CRITICAL: Required for large speed digits!
#define SMOOTH_FONT

// ------------------------------------------------------------------
// SPI Speed
// ------------------------------------------------------------------
#define SPI_FREQUENCY  27000000
#define SPI_READ_FREQUENCY  20000000
```
