/*
 *============================================================
 *  4ZE Racing — Formula Student EV Dashboard  v6.0
 *============================================================
 *
 *  Board  : ESP32 DevKit V1
 *  Display: ILI9488 480x320 (TFT_eSPI, HSPI)
 *  CAN    : MCP2515 500 kbps (VSPI - 3.3V Power Mode)
 *
 *  SPI Pin Assignment:
 *    TFT (HSPI) : SCK=14, MOSI=13, CS=15, DC=27, RST=33
 *    CAN (VSPI) : SCK=18, MISO=19, MOSI=23, CS=5, INT=4
 *============================================================
 */

#include "Config.h"
#include "Display.h"
#include "Dashboard.h"
#include "CANManager.h"

Dashboard dashboard;
static uint32_t heartbeatTimer = 0;
static bool     ledState       = false;

void heartbeat()
{
    const uint32_t now = millis();
    if (now - heartbeatTimer >= HEARTBEAT_INTERVAL_MS)
    {
        heartbeatTimer = now;
        ledState = !ledState;
        digitalWrite(PIN_LED, ledState);
    }
}

void setup()
{
    Serial.begin(115200);
    Serial.println();
    Serial.println(F("========================================"));
    Serial.println(F("   4ZE Racing — EV Dashboard v6.0"));
    Serial.println(F("   Production Universal CAN Engine (3.3V)"));
    Serial.println(F("========================================"));

    pinMode(PIN_LED, OUTPUT);

    // 1. Initialize CAN bus FIRST to lock hardware VSPI bus
    bool canOK = CANManager::init();

    // 2. Initialize Display on HSPI
    Display::init();
    Display::showBootScreen();
    Display::updateBootStatus("ESP32 Core", true);
    Display::updateBootStatus("MCP2515 CAN (VSPI)", canOK);
    delay(300);

    dashboard.init();
    Display::updateBootStatus("Dashboard Engine", true);
    Display::updateBootStatus("TFT Display (HSPI)", true);
    delay(500);

    Display::drawStaticUI();
    dashboard.forceFullRedraw();
    dashboard.update();

    Serial.println(F("[BOOT] System Ready. Entering main loop.\n"));
}

void loop()
{
    heartbeat();
    CANManager::pollParameters();              // Poll parameters via 0x201
    CANManager::receive(dashboard.getData());   // Receive & decipher CAN frames
    dashboard.update();                        // Draw on TFT immediately
    CANManager::checkTimeout(dashboard.getData());
}
