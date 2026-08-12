/*
 *============================================================
 *  4ZE Racing — CAN Bus Simulator v6.0 (Production)
 *============================================================
 *
 *  Board      : Arduino Nano (ATmega328P)
 *  CAN Module : MCP2515 on Hardware SPI (8 MHz crystal, 500 kbps)
 *
 *  Wiring (Arduino Nano -> MCP2515)
 *  ---------------------------------
 *  CS   : Pin D10
 *  MOSI : Pin D11
 *  MISO : Pin D12
 *  SCK  : Pin D13
 *  INT  : Pin D2 (Optional)
 *  VCC  : 5V / 3.3V
 *  GND  : GND (Connect to ESP32 Shared Common GND!)
 *============================================================
 */

#include <SPI.h>
#include <mcp_can.h>

constexpr uint8_t PIN_CAN_CS = 10;
constexpr uint8_t PIN_LED    = 13;

constexpr uint16_t CAN_ID_BMS_PACK_ELEC  = 0x007;
constexpr uint16_t CAN_ID_BMS_TEMPS      = 0x008;
constexpr uint16_t CAN_ID_BMS_PRECHARGE  = 0x009;
constexpr uint16_t CAN_ID_BMS_SOC_SOH    = 0x010;
constexpr uint16_t CAN_ID_VEHICLE_SPEED  = 0x100;
constexpr uint16_t CAN_ID_BAMO_REQUEST   = 0x201;
constexpr uint16_t CAN_ID_BAMO_RESPONSE  = 0x202;

constexpr unsigned long TELEMETRY_INTERVAL_MS = 100;
static unsigned long lastTelemetryMillis = 0;

MCP_CAN CAN(PIN_CAN_CS);

static uint16_t simulatedSpeed = 0;
static uint8_t  simulatedSOC   = 100;
static uint8_t  simulatedSOH   = 98;
static bool     accel          = true;

static void serialPrintf(const char *fmt, ...)
{
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Serial.print(buf);
}

void setup()
{
    Serial.begin(115200);
    while (!Serial) { ; }

    Serial.println();
    Serial.println(F("========================================"));
    Serial.println(F("   4ZE Racing — CAN Simulator v6.0 (Arduino Nano)"));
    Serial.println(F("========================================"));

    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);

    if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK)
    {
        Serial.println(F("[INIT] MCP2515 CAN Simulator : OK"));
    }
    else
    {
        Serial.println(F("[FATAL] MCP2515 Initialization Failed! Check Wiring."));
        while (true) {
            digitalWrite(PIN_LED, HIGH); delay(200);
            digitalWrite(PIN_LED, LOW);  delay(200);
        }
    }

    CAN.setMode(MCP_NORMAL);
    Serial.println(F("[READY] Transmitting simulated CAN data & listening for requests...\n"));
}

void loop()
{
    unsigned long now = millis();

    if (now - lastTelemetryMillis >= TELEMETRY_INTERVAL_MS)
    {
        updateSimulation();
        sendSimulatedTelemetry();
        sendOrionBMSBroadcast();
        lastTelemetryMillis = now;
    }

    checkIncomingRequests();
}

void updateSimulation()
{
    if (accel) {
        simulatedSpeed += 2;
        if (simulatedSpeed >= 160) accel = false;
    } else {
        simulatedSpeed -= 2;
        if (simulatedSpeed <= 0) {
            accel = true;
            simulatedSOC = (simulatedSOC > 15) ? (simulatedSOC - 1) : 100;
        }
    }
}

void sendSimulatedTelemetry()
{
    uint8_t payload[3];
    payload[0] = (uint8_t)(simulatedSpeed >> 8);
    payload[1] = (uint8_t)(simulatedSpeed & 0xFF);
    payload[2] = simulatedSOC;

    digitalWrite(PIN_LED, HIGH);
    CAN.sendMsgBuf(CAN_ID_VEHICLE_SPEED, 0, 3, payload);
    digitalWrite(PIN_LED, LOW);
}

void sendOrionBMSBroadcast()
{
    int16_t  packCurrentRaw  = (int16_t)map(simulatedSpeed, 0, 160, 0, 1800); // 0.1A
    uint16_t packVoltageRaw  = (uint16_t)(3900 - (simulatedSpeed * 2));         // 0.1V
    int16_t  highTempRaw     = 25 + (simulatedSpeed / 5);
    int16_t  lowTempRaw      = 22 + (simulatedSpeed / 8);
    uint16_t socRaw          = simulatedSOC * 2;                                // 0.5%
    uint16_t sohRaw          = simulatedSOH * 2;                                // 0.5%
    bool     prechargeState  = (simulatedSpeed > 5);

    byte d[8];

    // 0x007: Pack Current + Voltage
    d[0] = highByte(packCurrentRaw); d[1] = lowByte(packCurrentRaw);
    d[2] = highByte(packVoltageRaw); d[3] = lowByte(packVoltageRaw);
    CAN.sendMsgBuf(CAN_ID_BMS_PACK_ELEC, 0, 4, d);

    // 0x008: High Temp + Low Temp
    d[0] = highByte((uint16_t)highTempRaw); d[1] = lowByte((uint16_t)highTempRaw);
    d[2] = highByte((uint16_t)lowTempRaw);  d[3] = lowByte((uint16_t)lowTempRaw);
    CAN.sendMsgBuf(CAN_ID_BMS_TEMPS, 0, 4, d);

    // 0x009: Precharge state
    d[0] = prechargeState ? 1 : 0; d[1] = 0;
    CAN.sendMsgBuf(CAN_ID_BMS_PRECHARGE, 0, 2, d);

    // 0x010: SOC + SOH
    d[0] = highByte(socRaw); d[1] = lowByte(socRaw);
    d[2] = highByte(sohRaw); d[3] = lowByte(sohRaw);
    CAN.sendMsgBuf(CAN_ID_BMS_SOC_SOH, 0, 4, d);
}

void checkIncomingRequests()
{
    if (CAN.checkReceive() != CAN_MSGAVAIL) return;

    unsigned long rxID  = 0;
    unsigned char rxLen = 0;
    unsigned char rxBuf[8];

    CAN.readMsgBuf(&rxID, &rxLen, rxBuf);

    if (rxID == CAN_ID_BAMO_REQUEST && rxLen >= 2)
    {
        uint8_t paramID = rxBuf[1];
        int16_t val     = 0;

        if (paramID == 0xEB)       val = (int16_t)map(simulatedSpeed, 0, 160, 0, 140);
        else if (paramID == 0x4A)  val = 30 + (simulatedSpeed / 3);
        else if (paramID == 0x49)  val = 25 + (simulatedSpeed / 5);
        else if (paramID == 0x28)  val = 390 - (simulatedSpeed / 4);
        else if (paramID == 0x27)  val = map(simulatedSpeed, 0, 160, 0, 180);
        else if (paramID == 0x30)  val = (simulatedSpeed > 5) ? 1 : 0;
        else if (paramID == 0x4D)  val = simulatedSOH;
        else                      val = (int16_t)(millis() & 0xFF);

        uint8_t responseBuf[3] = { paramID, highByte((uint16_t)val), lowByte((uint16_t)val) };

        digitalWrite(PIN_LED, HIGH);
        CAN.sendMsgBuf(CAN_ID_BAMO_RESPONSE, 0, 3, responseBuf);
        digitalWrite(PIN_LED, LOW);

        serialPrintf("[RX REQ -> TX RESP] Param:0x%02X -> Val:%d\n", paramID, val);
    }
}
