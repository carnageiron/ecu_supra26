/*
 *============================================================
 *  4ZE Racing — CAN Data Logger  v2.0
 *============================================================
 *
 *  Board      : ESP32 DevKit V1
 *  SD Module  : SPI SD Card on HSPI
 *  CAN Module : MCP2515 on VSPI  (8 MHz crystal, 500 kbps)
 *
 *  Wiring
 *  ──────
 *  MCP2515 (VSPI)        SD Card (HSPI)
 *    MOSI : GPIO23         MOSI : GPIO13
 *    MISO : GPIO19         MISO : GPIO2
 *    SCK  : GPIO18         SCK  : GPIO14
 *    CS   : GPIO5          CS   : GPIO15
 *
 *  Purpose
 *  -------
 *  Log every received CAN frame (including BMS telemetry messages)
 *  to a CSV file on the SD card with full 8-byte payload support.
 *
 *  BMS CAN Telemetry Handled:
 *   - 0x007: Pack Current & Pack Instantaneous Voltage (DLC 4)
 *   - 0x072: Precharge Resistor Temp (DLC 6)
 *   - 0x181: Application Diagnostic Status (DLC 8)
 *   - 0x008: High Temp & Low Temp (DLC 4)
 *   - 0x009: Precharge Circuit Status (DLC 2)
 *   - 0x010: Pack SOC & Pack SOH (DLC 4)
 *
 *  CSV Schema (LOGxxxx/supra_log.csv)
 *  ──────────────────────────────────
 *  ID, Length, Byte0, Byte1, Byte2, Byte3, Byte4, Byte5, Byte6, Byte7, Timestamp_ms
 *============================================================
 */

#include <SD.h>
#include <SPI.h>
#include <mcp_can.h>

//------------------------------------------------------------
//  Pin Configuration
//------------------------------------------------------------

constexpr uint8_t PIN_CAN_CS = 5;
constexpr uint8_t PIN_SD_CS  = 15;
constexpr uint8_t PIN_LED    = 2;

// HSPI bus pins for SD card
constexpr uint8_t PIN_SD_SCK  = 14;
constexpr uint8_t PIN_SD_MISO = 2;
constexpr uint8_t PIN_SD_MOSI = 13;

//------------------------------------------------------------
//  CAN Request Scheduler — Constants
//------------------------------------------------------------

// Target CAN ID for outgoing parameter requests
constexpr uint16_t CAN_ID_REQUEST = 0x201;

// Fixed framing bytes for the 3-byte request payload
constexpr uint8_t REQ_PREFIX = 0x3D;
constexpr uint8_t REQ_SUFFIX = 0x64;

// Time between successive parameter requests (milliseconds)
// Set to 500 ms to avoid CAN TX buffer congestion (MCP2515 err 7)
constexpr unsigned long REQUEST_INTERVAL_MS = 500;

// Parameter IDs to cycle through (includes synchronized BMS param IDs)
static const uint8_t parameterIDs[] = {
    0x07, 0x72, 0x81, 0x08, 0x09, 0x10, 0xEE, 0x27, 0x28, 0x49, 0x4A, 0x4D, 0x4E, 0x30, 0xEB
};
constexpr uint8_t NUM_PARAMETERS = sizeof(parameterIDs) / sizeof(parameterIDs[0]);

//------------------------------------------------------------
//  CAN Request Scheduler — State
//------------------------------------------------------------

static uint8_t currentParamIndex    = 0;       // Index into parameterIDs[]
static unsigned long lastRequestMillis = 0;   // Timestamp of last request

//------------------------------------------------------------
//  Session Folder & Log File Path
//------------------------------------------------------------

// Dynamically built at boot, e.g. "/LOG0042/supra_log.csv"
static char logFilePath[32];

//------------------------------------------------------------
//  Hardware Instances
//------------------------------------------------------------

SPIClass hspi(HSPI);
MCP_CAN CAN(PIN_CAN_CS);

//------------------------------------------------------------
//  Statistics
//------------------------------------------------------------

static uint32_t frameCount = 0;

//------------------------------------------------------------
//  Function Prototypes
//------------------------------------------------------------

bool initSDCard();
bool initCAN();
void createSessionFolder();
void ensureCSVHeader();
void sendNextCANRequest();
void logCANFrame(unsigned long id, uint8_t len, const uint8_t *buf);
void printBMSParsedDebug(unsigned long id, uint8_t len, const uint8_t *buf);

//============================================================
//  SETUP
//============================================================

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("========================================"));
  Serial.println(F("   4ZE Racing — CAN Data Logger v2.0    "));
  Serial.println(F("========================================"));

  pinMode(PIN_LED, OUTPUT);

  // Keep both CS lines HIGH during init to avoid bus conflict
  pinMode(PIN_CAN_CS, OUTPUT);
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_CAN_CS, HIGH);
  digitalWrite(PIN_SD_CS, HIGH);
  delay(100);

  // ---- SD Card (HSPI) ----
  bool sdOK = initSDCard();
  Serial.print(F("[INIT] SD Card : "));
  Serial.println(sdOK ? F("OK") : F("FAILED"));

  if (sdOK) {
    createSessionFolder(); // Build /LOGxxxx/ and set logFilePath
    ensureCSVHeader();
  }

  // ---- CAN Bus (VSPI) ----
  bool canOK = initCAN();
  Serial.print(F("[INIT] CAN Bus : "));
  Serial.println(canOK ? F("OK") : F("FAILED"));

  if (!canOK) {
    Serial.println(F("[FATAL] Cannot continue without CAN. Halting."));
    while (true) {
      delay(1000);
    }
  }

  lastRequestMillis = millis();

  Serial.println();
  Serial.println(F("[READY] Logging CAN frames to SD card..."));
  Serial.printf("[READY] Session path: %s\n", logFilePath);
  Serial.println();
}

//============================================================
//  MAIN LOOP
//============================================================

void loop() {
  // ---- Non-blocking CAN request scheduler ----
  unsigned long now = millis();
  if (now - lastRequestMillis >= REQUEST_INTERVAL_MS) {
    sendNextCANRequest();
    lastRequestMillis = now;
  }

  // ---- Receive & log incoming CAN frames ----
  if (CAN.checkReceive() == CAN_MSGAVAIL) {
    unsigned long rxID = 0;
    unsigned char rxLen = 0;
    unsigned char rxBuf[8];

    CAN.readMsgBuf(&rxID, &rxLen, rxBuf);

    // LED flash to indicate activity
    digitalWrite(PIN_LED, HIGH);

    frameCount++;

    // ---- Log frame to SD ----
    logCANFrame(rxID, rxLen, rxBuf);

    digitalWrite(PIN_LED, LOW);
  }
}

//============================================================
//  SD Card Initialisation
//============================================================

bool initSDCard() {
  hspi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  return SD.begin(PIN_SD_CS, hspi);
}

//============================================================
//  CAN Bus Initialisation
//============================================================

bool initCAN() {
  if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) != CAN_OK) {
    return false;
  }
  CAN.setMode(MCP_NORMAL);
  return true;
}

//============================================================
//  Session Folder Creation
//============================================================

void createSessionFolder() {
  uint16_t highestSession = 0;

  File root = SD.open("/");
  if (root) {
    File entry = root.openNextFile();
    while (entry) {
      if (entry.isDirectory()) {
        const char *name = entry.name();
        const char *base = name;
        if (base[0] == '/') {
          base++;
        }

        if (strlen(base) == 7 && base[0] == 'L' && base[1] == 'O' && base[2] == 'G') {
          uint16_t num = (uint16_t)atoi(base + 3);
          if (num > highestSession) {
            highestSession = num;
          }
        }
      }
      entry.close();
      entry = root.openNextFile();
    }
    root.close();
  }

  uint16_t nextSession = highestSession + 1;

  char folderPath[12];
  snprintf(folderPath, sizeof(folderPath), "/LOG%04u", nextSession);

  if (SD.mkdir(folderPath)) {
    Serial.printf("[SD] Created session folder: %s\n", folderPath);
  } else {
    Serial.printf("[SD] WARNING: Could not create %s\n", folderPath);
  }

  snprintf(logFilePath, sizeof(logFilePath), "%s/supra_log.csv", folderPath);
}

//============================================================
//  CAN Request Scheduler
//============================================================

void sendNextCANRequest() {
  uint8_t requestPayload[3] = {REQ_PREFIX, parameterIDs[currentParamIndex], REQ_SUFFIX};

  uint8_t result = CAN.sendMsgBuf(CAN_ID_REQUEST, 0, 3, requestPayload);

  if (result == CAN_OK) {
    Serial.printf("[CAN TX] ID:0x%03X  Param: 0x%02X\n", CAN_ID_REQUEST, parameterIDs[currentParamIndex]);
    currentParamIndex = (currentParamIndex + 1) % NUM_PARAMETERS;
  } else {
    // TX buffer busy / error: log debug and retry this parameter on next tick
    Serial.printf("[CAN TX] BUSY/FAILED param 0x%02X (err %d) — will retry\n",
                  parameterIDs[currentParamIndex], result);
  }
}

//============================================================
//  CSV Header
//============================================================

void ensureCSVHeader() {
  if (SD.exists(logFilePath)) {
    Serial.println(F("[SD] Existing log file found — appending."));
    return;
  }

  File f = SD.open(logFilePath, FILE_WRITE);
  if (f) {
    f.println(F("ID,Length,Byte0,Byte1,Byte2,Byte3,Byte4,Byte5,Byte6,Byte7,Timestamp_ms"));
    f.close();
    Serial.println(F("[SD] New log file created with 8-byte CSV header."));
  }
}

//============================================================
//  Log: CAN Frame (Full 8-Byte Payload Support)
//============================================================

void logCANFrame(unsigned long id, uint8_t len, const uint8_t *buf) {
  uint8_t b[8] = {0};
  for (uint8_t i = 0; i < len && i < 8; i++) {
    b[i] = buf[i];
  }

  // Serial debug output
  Serial.printf("[%06lu] #%lu  ID:0x%03lX  Len:%d  Data: %02X %02X %02X %02X %02X %02X %02X %02X\n",
                millis(), frameCount, id, len, b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);

  printBMSParsedDebug(id, len, b);

  // Write formatted row to SD Card CSV
  File f = SD.open(logFilePath, FILE_APPEND);
  if (f) {
    char csvLine[128];
    snprintf(csvLine, sizeof(csvLine), "0x%03lX,%d,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%lu",
             id, len, b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], millis());
    f.println(csvLine);
    f.close();
  }
}

//============================================================
//  Parsed BMS Serial Debug Output
//============================================================

void printBMSParsedDebug(unsigned long id, uint8_t len, const uint8_t *b) {
  switch (id) {
    case 0x007: {
      int16_t current = (int16_t)((b[0] << 8) | b[1]);
      uint16_t voltage = (uint16_t)((b[2] << 8) | b[3]);
      Serial.printf("        ↳ [BMS 0x007] Current: %.1f A | Inst Voltage: %.1f V\n",
                    current / 10.0, voltage / 10.0);
      break;
    }
    case 0x072: {
      int16_t resTemp = (int16_t)((b[2] << 8) | b[3]);
      Serial.printf("        ↳ [BMS 0x072] Precharge Resistor Temp: %d °C\n", resTemp);
      break;
    }
    case 0x181: {
      uint16_t appDiag = (uint16_t)((b[2] << 8) | b[3]);
      Serial.printf("        ↳ [BMS 0x181] Application Diagnostic Status: 0x%04X\n", appDiag);
      break;
    }
    case 0x008: {
      int16_t highTemp = (int16_t)((b[0] << 8) | b[1]);
      int16_t lowTemp  = (int16_t)((b[2] << 8) | b[3]);
      Serial.printf("        ↳ [BMS 0x008] High Temp: %d °C | Low Temp: %d °C\n", highTemp, lowTemp);
      break;
    }
    case 0x009: {
      uint16_t prechargeStat = (uint16_t)((b[0] << 8) | b[1]);
      Serial.printf("        ↳ [BMS 0x009] Precharge Circuit Status: %u\n", prechargeStat);
      break;
    }
    case 0x010: {
      uint16_t soc = (uint16_t)((b[0] << 8) | b[1]);
      uint16_t soh = (uint16_t)((b[2] << 8) | b[3]);
      Serial.printf("        ↳ [BMS 0x010] Pack SOC: %u %% | Pack Health (SOH): %u %%\n", soc, soh);
      break;
    }
    default:
      break;
  }
}
