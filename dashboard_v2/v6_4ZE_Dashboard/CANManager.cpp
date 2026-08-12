#include "CANManager.h"
#include <SPI.h>
#include <mcp_can.h>

static MCP_CAN canBus(PIN_CAN_CS);
static uint32_t lastFrameTimestamp = 0;
static bool canInitSuccess = false;

static uint32_t lastPollMillis = 0;
static uint32_t lastWaitingDiagMillis = 0;

static const uint8_t pollRegs[] = {
    REG_TORQUE, REG_MC_TEMP, REG_BAT_TEMP,
    REG_VOLTAGE, REG_CURRENT, REG_PRECHG, REG_SOH
};
static uint8_t pollIdx = 0;

//------------------------------------------------------------

bool CANManager::init()
{
    Serial.println(F("\n========================================"));
    Serial.println(F("   MCP2515 SPI Hardware Diagnostics"));
    Serial.println(F("========================================"));

    // 1. Force VSPI bus pins explicitly: SCK=18, MISO=19, MOSI=23, CS=5
    SPI.begin(18, 19, 23, PIN_CAN_CS);
    SPI.setFrequency(2000000); // 2 MHz safe SPI clock rate for 3.3V logic

    pinMode(PIN_CAN_CS, OUTPUT);
    digitalWrite(PIN_CAN_CS, HIGH);
    delay(100);

    // 2. Perform SPI probe reads from MCP2515 CANSTAT register (0x0E)
    uint8_t probeByte = 0;
    for (int i = 1; i <= 3; i++)
    {
        digitalWrite(PIN_CAN_CS, LOW);
        SPI.transfer(0xC0); // Reset
        digitalWrite(PIN_CAN_CS, HIGH);
        delay(20);

        digitalWrite(PIN_CAN_CS, LOW);
        SPI.transfer(0x03); // Read Command
        SPI.transfer(0x0E); // CANSTAT Register Address
        probeByte = SPI.transfer(0x00);
        digitalWrite(PIN_CAN_CS, HIGH);

        Serial.printf("[SPI PROBE #%d] CS Pin: %d | Read CANSTAT Reg: 0x%02X\n", i, PIN_CAN_CS, probeByte);
        delay(50);
    }

    // 3. Initialize via mcp_can library (Try 8MHz, then 16MHz)
    if (canBus.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK)
    {
        canBus.setMode(MCP_NORMAL);
        canInitSuccess = true;
        Serial.println(F("Entering Configuration Mode Successful!"));
        Serial.println(F("Setting Baudrate Successful!"));
        Serial.println(F("[CAN OK] MCP2515 initialized (8 MHz crystal).\n"));
        return true;
    }

    delay(100);
    if (canBus.begin(MCP_ANY, CAN_500KBPS, MCP_16MHZ) == CAN_OK)
    {
        canBus.setMode(MCP_NORMAL);
        canInitSuccess = true;
        Serial.println(F("Entering Configuration Mode Successful!"));
        Serial.println(F("Setting Baudrate Successful!"));
        Serial.println(F("[CAN OK] MCP2515 initialized (16 MHz crystal).\n"));
        return true;
    }

    Serial.println(F("[CAN ERROR] mcp_can begin() failed.\n"));
    canInitSuccess = false;
    return false;
}

//------------------------------------------------------------
//  Poll parameters on CAN ID 0x201 (Continuous 200ms cycle)
//------------------------------------------------------------

void CANManager::pollParameters()
{
    if (!canInitSuccess) return;

    uint32_t now = millis();
    if (now - lastPollMillis >= 200)
    {
        lastPollMillis = now;
        uint8_t req[3] = { 0x3D, pollRegs[pollIdx], 0x64 };

        SPI.begin(18, 19, 23, PIN_CAN_CS);
        SPI.setFrequency(2000000);
        uint8_t res = canBus.sendMsgBuf(CAN_ID_BAMO_REQUEST, 0, 3, req);

        if (res == CAN_OK) {
            Serial.printf("[CAN TX REQ -> APPS] ID:0x201 Param:0x%02X\n", pollRegs[pollIdx]);
        } else {
            Serial.printf("[CAN TX FAIL -> APPS] ID:0x201 Param:0x%02X (Err: %d)\n", pollRegs[pollIdx], res);
        }

        pollIdx = (pollIdx + 1) % (sizeof(pollRegs) / sizeof(pollRegs[0]));
    }
}

//------------------------------------------------------------
//  Receive and decipher CAN frames from APPS Simulator / BMS
//------------------------------------------------------------

void CANManager::receive(DashboardData& data)
{
    if (!canInitSuccess) return;

    SPI.begin(18, 19, 23, PIN_CAN_CS);
    SPI.setFrequency(2000000);

    // If no message available, print waiting diagnostic every 2 seconds
    if (canBus.checkReceive() != CAN_MSGAVAIL)
    {
        uint32_t now = millis();
        if (lastFrameTimestamp == 0 && now - lastWaitingDiagMillis >= 2000)
        {
            lastWaitingDiagMillis = now;
            Serial.println(F("[CAN WAITING FOR APPS DATA...] No CAN frames received yet. Please check:"));
            Serial.println(F("  1. Is Arduino Nano Simulator running and Pin 13 LED blinking?"));
            Serial.println(F("  2. Are CAN_H and CAN_L wired between Nano MCP2515 & ESP32 MCP2515? (Try swapping CAN_H/CAN_L if reversed!)"));
            Serial.println(F("  3. Is there a shared common GND wire between Nano and ESP32?"));
        }
        return;
    }

    unsigned long rxID  = 0;
    unsigned char rxLen = 0;
    unsigned char rxBuf[8];

    canBus.readMsgBuf(&rxID, &rxLen, rxBuf);

    data.frameCounter++;
    data.lastCANID     = static_cast<uint32_t>(rxID);
    data.canOnline     = true;
    lastFrameTimestamp  = millis();

    // ---- PROMINENT APPS CAN RECEIVE LOG ----
    Serial.printf("\n>>> [CAN DATA RECEIVED FROM APPS!] <<< Frame #%lu | ID: 0x%03lX | Len: %d | Data: %02X %02X %02X\n",
                  data.frameCounter, rxID, rxLen,
                  (rxLen > 0) ? rxBuf[0] : 0,
                  (rxLen > 1) ? rxBuf[1] : 0,
                  (rxLen > 2) ? rxBuf[2] : 0);

    // ---- 0x100: Speed (Bytes 0-1) + SOC (Byte 2) ----
    if (rxID == CAN_ID_VEHICLE_SPEED && rxLen >= 2)
    {
        data.speed = (uint16_t(rxBuf[0]) << 8) | rxBuf[1];
        if (rxLen >= 3)
        {
            data.soc = rxBuf[2];
        }
        Serial.printf(" -> [PARSED TELEMETRY] Speed: %u km/h | SOC: %u%%\n", data.speed, data.soc);
    }

    // ---- 0x202: Parameter Responses ----
    if (rxID == CAN_ID_BAMO_RESPONSE && rxLen >= 3)
    {
        uint8_t paramID = rxBuf[0];
        int16_t value   = (int16_t)((uint16_t(rxBuf[1]) << 8) | rxBuf[2]);

        switch (paramID)
        {
            case REG_TORQUE:    data.motorTorque = value; Serial.printf(" -> [PARSED BAMO] Torque: %d Nm\n", value); break;
            case REG_MC_TEMP:   data.mcTemp      = value; Serial.printf(" -> [PARSED BAMO] MC Temp: %d deg C\n", value); break;
            case REG_BAT_TEMP:  data.batteryTemp = value; Serial.printf(" -> [PARSED BAMO] Batt Temp: %d deg C\n", value); break;
            case REG_VOLTAGE:   data.packVoltage = (uint16_t)value; Serial.printf(" -> [PARSED BAMO] Pack Voltage: %u V\n", value); break;
            case REG_CURRENT:   data.packCurrent = value; Serial.printf(" -> [PARSED BAMO] Pack Current: %d A\n", value); break;
            case REG_PRECHG:    data.precharge   = (value != 0); Serial.printf(" -> [PARSED BAMO] Precharge: %s\n", value ? "ACTIVE" : "OFF"); break;
            case REG_SOH:       data.soh         = (uint8_t)value; Serial.printf(" -> [PARSED BAMO] SOH: %u%%\n", value); break;
        }
    }

    // ---- 0x007: Pack Current + Pack Voltage (Orion BMS Direct) ----
    if (rxID == CAN_ID_BMS_PACK_ELEC && rxLen >= 4)
    {
        int16_t  rawCur = (int16_t)((uint16_t(rxBuf[0]) << 8) | rxBuf[1]);
        uint16_t rawVol = (uint16_t(rxBuf[2]) << 8) | rxBuf[3];

        data.packCurrent = rawCur / BMS_CURRENT_DIV;
        data.packVoltage = rawVol / BMS_VOLTAGE_DIV;
        Serial.printf(" -> [PARSED BMS 0x007] Current: %d A | Voltage: %u V\n", data.packCurrent, data.packVoltage);
    }

    // ---- 0x008: High Temp (Orion BMS Direct) ----
    if (rxID == CAN_ID_BMS_TEMPS && rxLen >= 4)
    {
        data.batteryTemp = (int16_t)((uint16_t(rxBuf[0]) << 8) | rxBuf[1]);
        Serial.printf(" -> [PARSED BMS 0x008] High Temp: %d deg C\n", data.batteryTemp);
    }

    // ---- 0x009: Precharge Circuit (Orion BMS Direct) ----
    if (rxID == CAN_ID_BMS_PRECHARGE && rxLen >= 1)
    {
        data.precharge = (rxBuf[0] != 0);
        Serial.printf(" -> [PARSED BMS 0x009] Precharge: %s\n", data.precharge ? "ACTIVE" : "OFF");
    }

    // ---- 0x010: SOC + SOH (Orion BMS Direct) ----
    if (rxID == CAN_ID_BMS_SOC_SOH && rxLen >= 4)
    {
        uint16_t rawSOC = (uint16_t(rxBuf[0]) << 8) | rxBuf[1];
        uint16_t rawSOH = (uint16_t(rxBuf[2]) << 8) | rxBuf[3];

        data.soc = (uint8_t)(rawSOC / BMS_SOC_DIV);
        data.soh = (uint8_t)(rawSOH / BMS_SOH_DIV);
        Serial.printf(" -> [PARSED BMS 0x010] SOC: %u%% | SOH: %u%%\n", data.soc, data.soh);
    }
}

//------------------------------------------------------------

void CANManager::checkTimeout(DashboardData& data)
{
    if (lastFrameTimestamp == 0) { data.canOnline = false; return; }
    if (millis() - lastFrameTimestamp > CAN_TIMEOUT_MS)
        data.canOnline = false;
}

uint32_t CANManager::getLastFrameTime() { return lastFrameTimestamp; }
