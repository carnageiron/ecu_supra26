#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

/*
 *============================================================
 *  4ZE Racing Formula Student EV Dashboard  v6.0
 *  Configuration Header
 *============================================================
 *
 *  Hardware Layout (ESP32 DevKit V1):
 *
 *  TFT Display (ILI9488 on HSPI):
 *    - SCK  : GPIO 14
 *    - MOSI : GPIO 13
 *    - MISO : GPIO 12 (Optional)
 *    - CS   : GPIO 15
 *    - DC   : GPIO 27
 *    - RST  : GPIO 33
 *    - VCC  : 5V / 3.3V
 *    - GND  : GND
 *
 *  CAN Controller (MCP2515 on VSPI - 3.3V Power Compliant):
 *    - SCK  : GPIO 18
 *    - MISO : GPIO 19
 *    - MOSI : GPIO 23
 *    - CS   : GPIO 5
 *    - INT  : GPIO 4
 *    - VCC  : 3.3V / 5V
 *    - GND  : GND (Shared Common GND across all nodes!)
 *============================================================
 */

//------------------------------------------------------------
//  Hardware Pin Definitions — MCP2515 (VSPI Bus)
//------------------------------------------------------------

constexpr uint8_t PIN_CAN_CS  = 5;     ///< MCP2515 Chip Select (VSPI)
constexpr uint8_t PIN_CAN_INT = 4;     ///< MCP2515 Interrupt
constexpr uint8_t PIN_LED     = 2;     ///< Onboard Heartbeat LED

// SPI Clock Frequency for MCP2515 (4 MHz for 3.3V logic stability)
constexpr uint32_t MCP_SPI_CLOCK_FREQ = 4000000;

//------------------------------------------------------------
//  CAN Bus — Message IDs
//------------------------------------------------------------

constexpr uint16_t CAN_ID_BMS_PACK_ELEC  = 0x007;  ///< Orion BMS Current + Voltage
constexpr uint16_t CAN_ID_BMS_TEMPS      = 0x008;  ///< Orion BMS High Temp + Low Temp
constexpr uint16_t CAN_ID_BMS_PRECHARGE  = 0x009;  ///< Orion BMS Precharge Relay
constexpr uint16_t CAN_ID_BMS_SOC_SOH    = 0x010;  ///< Orion BMS SOC + SOH

constexpr uint16_t CAN_ID_VEHICLE_SPEED  = 0x100;  ///< Speed (Bytes 0-1) + SOC (Byte 2)
constexpr uint16_t CAN_ID_BAMO_REQUEST   = 0x201;  ///< Request ID sent to Simulator/Bamocar
constexpr uint16_t CAN_ID_BAMO_RESPONSE  = 0x202;  ///< Response ID from Simulator/Bamocar

//------------------------------------------------------------
//  Parameter Register IDs (Queried on 0x201, answered on 0x202)
//------------------------------------------------------------

constexpr uint8_t REG_TORQUE   = 0xEB;  ///< Motor Torque
constexpr uint8_t REG_MC_TEMP  = 0x4A;  ///< Motor Controller Temp (IGBT)
constexpr uint8_t REG_BAT_TEMP = 0x49;  ///< Battery / Motor Temp
constexpr uint8_t REG_VOLTAGE  = 0x28;  ///< Pack Voltage
constexpr uint8_t REG_CURRENT  = 0x27;  ///< Pack / Phase Current
constexpr uint8_t REG_PRECHG   = 0x30;  ///< Precharge Relay status
constexpr uint8_t REG_SOH      = 0x4D;  ///< Battery State of Health

//------------------------------------------------------------
//  Orion BMS Scaling (Integer Arithmetic)
//------------------------------------------------------------

constexpr int BMS_CURRENT_DIV  = 10;   ///< Raw / 10 = Amps
constexpr int BMS_VOLTAGE_DIV  = 10;   ///< Raw / 10 = Volts
constexpr int BMS_SOC_DIV      = 2;    ///< Raw / 2  = Percent
constexpr int BMS_SOH_DIV      = 2;    ///< Raw / 2  = Percent

//------------------------------------------------------------
//  Timing Constants
//------------------------------------------------------------

constexpr uint32_t CAN_TIMEOUT_MS            = 1000;
constexpr uint32_t HEARTBEAT_INTERVAL_MS     = 500;
constexpr uint32_t DISPLAY_UPDATE_INTERVAL_MS = 50;

//------------------------------------------------------------
//  Screen Dimensions (ILI9488 480x320 Landscape)
//------------------------------------------------------------

constexpr int SCREEN_W = 480;
constexpr int SCREEN_H = 320;
constexpr int VDIV_X   = 240;

//------------------------------------------------------------
//  LEFT Panel (Telemetry & Battery)
//------------------------------------------------------------

constexpr int LEFT_X = 0;
constexpr int LEFT_W = 239;

constexpr int TELEM_START_Y    = 0;
constexpr int TELEM_ROW_H      = 43;
constexpr int TELEM_LABEL_Y_OS = 5;
constexpr int TELEM_VALUE_Y_OS = 24;
constexpr int TELEM_LABEL_X    = 10;
constexpr int TELEM_VALUE_X    = LEFT_W - 8;

#define TELEM_ROW_Y(i) (TELEM_START_Y + (i) * TELEM_ROW_H)

constexpr int LEFT_HDIV_Y = TELEM_START_Y + 4 * TELEM_ROW_H;  // 172

constexpr int SOC_BAR_Y   = LEFT_HDIV_Y + 18;
constexpr int SOH_BAR_Y   = LEFT_HDIV_Y + 58;

constexpr int BAR_LABEL_X = 10;
constexpr int BAR_X       = 48;
constexpr int BAR_W       = 138;
constexpr int BAR_H       = 22;
constexpr int BAR_RADIUS  = 3;
constexpr int BAR_PCT_X   = BAR_X + BAR_W + 8;

constexpr int BATT_ICON_X = 65;
constexpr int BATT_ICON_Y = LEFT_HDIV_Y + 105;
constexpr int BATT_ICON_W = 100;
constexpr int BATT_ICON_H = 28;

//------------------------------------------------------------
//  RIGHT Panel (Speed, Precharge, Logo, Torque)
//------------------------------------------------------------

constexpr int RIGHT_X = VDIV_X + 1;
constexpr int RIGHT_W = SCREEN_W - RIGHT_X;

constexpr int CAN_STATUS_X = RIGHT_X + RIGHT_W - 100;
constexpr int CAN_STATUS_Y = 10;

constexpr int SPEED_CX     = RIGHT_X + RIGHT_W / 2;
constexpr int SPEED_NUM_Y  = 40;
constexpr int SPEED_UNIT_Y = 140;

constexpr int PRE_IND_CX   = SPEED_CX;
constexpr int PRE_IND_Y    = 186;
constexpr int PRE_DOT_R    = 7;

constexpr int LOGO_CX      = SPEED_CX;
constexpr int LOGO_Y       = 216;
constexpr int LOGO_W       = 80;
constexpr int LOGO_H       = 26;

constexpr int RIGHT_HDIV_Y  = 252;
constexpr int TORQUE_LBL_Y  = RIGHT_HDIV_Y + 12;
constexpr int TORQUE_VAL_Y  = RIGHT_HDIV_Y + 12;
constexpr int BOTTOM_ROW2_Y  = RIGHT_HDIV_Y + 38;
constexpr int BOTTOM_LEFT_X  = RIGHT_X + 10;
constexpr int BOTTOM_RIGHT_X = RIGHT_X + RIGHT_W - 10;

//------------------------------------------------------------
//  Colour Palette
//------------------------------------------------------------

#define RGB565(r, g, b) \
    static_cast<uint16_t>((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

constexpr uint16_t COLOR_BG             = 0x0000;
constexpr uint16_t COLOR_PANEL_BG       = 0x10A2;
constexpr uint16_t COLOR_DIVIDER        = 0x2945;
constexpr uint16_t COLOR_BORDER         = 0x4208;
constexpr uint16_t COLOR_TEXT_PRIMARY    = 0xFFFF;
constexpr uint16_t COLOR_TEXT_SECONDARY  = 0xB596;
constexpr uint16_t COLOR_TEXT_DIM        = 0x6B6D;
constexpr uint16_t COLOR_ACCENT         = 0xFD20;
constexpr uint16_t COLOR_ACCENT_DIM     = 0x8200;
constexpr uint16_t COLOR_GREEN          = 0x07E0;
constexpr uint16_t COLOR_YELLOW         = 0xFFE0;
constexpr uint16_t COLOR_RED            = 0xF800;
constexpr uint16_t COLOR_CYAN           = 0x07FF;
constexpr uint16_t COLOR_BAR_BG         = 0x18E3;
constexpr uint16_t COLOR_BAR_BORDER     = 0x3186;

enum class DashboardPage : uint8_t {
    DRIVER = 0, BATTERY, MOTOR, TEMPERATURES, FAULTS, GPS, PAGE_COUNT
};

#endif // CONFIG_H
