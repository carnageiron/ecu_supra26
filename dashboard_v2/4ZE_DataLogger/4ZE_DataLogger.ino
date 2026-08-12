/*
 *============================================================
 *  4ZE Racing — CAN Data Logger  v6.0
 *============================================================
 *
 *  Board      : ESP32 DevKit V1
 *  SD Module  : SPI SD Card on HSPI
 *  CAN Module : MCP2515 on VSPI  (8 MHz crystal, 500 kbps)
 *
 *  Wiring
 *  ------
 *  MCP2515 (VSPI)        SD Card (HSPI)
 *    MOSI : GPIO23         MOSI : GPIO13
 *    MISO : GPIO19         MISO : GPIO2
 *    SCK  : GPIO18         SCK  : GPIO14
 *    CS   : GPIO5          CS   : GPIO15
 *============================================================
 */

#include <SD.h>
#include <SPI.h>
#include <mcp_can.h>

constexpr uint8_t PIN_CAN_CS = 5;
constexpr uint8_t PIN_SD_CS  = 15;
constexpr uint8_t PIN_LED    = 2;

constexpr uint8_t PIN_SD_SCK  = 14;
constexpr uint8_t PIN_SD_MISO = 2;
constexpr uint8_t PIN_SD_MOSI = 13;

constexpr uint16_t CAN_ID_REQUEST = 0x201;

constexpr uint8_t REQ_PREFIX = 0x3D;
constexpr uint8_t REQ_SUFFIX = 0x64;
constexpr unsigned long REQUEST_INTERVAL_MS = 100;

static const uint8_t parameterIDs[] = {0xEE, 0x27, 0x28, 0x49, 0x4A,
                                       0x4D, 0x4E, 0x30, 0xEB};
constexpr uint8_t NUM_PARAMETERS =
    sizeof(parameterIDs) / sizeof(parameterIDs[0]);

static uint8_t currentParamIndex = 0;
static unsigned long lastRequestMillis = 0;

static char logFilePath[32];

SPIClass hspi(HSPI);
MCP_CAN CAN(PIN_CAN_CS);

static uint32_t frameCount = 0;

bool initSDCard();
bool initCAN();
void createSessionFolder();
void ensureCSVHeader();
void sendNextCANRequest();
void logCANFrame(unsigned long id, uint8_t len, const uint8_t *buf);

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("========================================"));
  Serial.println(F("   4ZE Racing — Data Logger v6.0"));
  Serial.println(F("========================================"));

  pinMode(PIN_LED, OUTPUT);

  pinMode(PIN_CAN_CS, OUTPUT);
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_CAN_CS, HIGH);
  digitalWrite(PIN_SD_CS, HIGH);
  delay(100);

  bool sdOK = initSDCard();
  Serial.print(F("[INIT] SD Card : "));
  Serial.println(sdOK ? F("OK") : F("FAILED"));

  if (sdOK) {
    createSessionFolder();
    ensureCSVHeader();
  }

  bool canOK = initCAN();
  Serial.print(F("[INIT] CAN Bus : "));
  Serial.println(canOK ? F("OK") : F("FAILED"));

  if (!canOK) {
    Serial.println(F("[FATAL] Cannot continue without CAN. Halting."));
    while (true) { delay(1000); }
  }

  lastRequestMillis = millis();

  Serial.println();
  Serial.println(F("[READY] Logging CAN frames to SD card..."));
  Serial.printf("[READY] Session path: %s\n", logFilePath);
  Serial.println();
}

void loop() {
  unsigned long now = millis();
  if (now - lastRequestMillis >= REQUEST_INTERVAL_MS) {
    sendNextCANRequest();
    lastRequestMillis = now;
  }

  if (CAN.checkReceive() == CAN_MSGAVAIL) {
    unsigned long rxID = 0;
    unsigned char rxLen = 0;
    unsigned char rxBuf[8];

    CAN.readMsgBuf(&rxID, &rxLen, rxBuf);

    digitalWrite(PIN_LED, HIGH);
    frameCount++;
    logCANFrame(rxID, rxLen, rxBuf);
    digitalWrite(PIN_LED, LOW);
  }
}

bool initSDCard() {
  hspi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  return SD.begin(PIN_SD_CS, hspi);
}

bool initCAN() {
  if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) != CAN_OK) {
    delay(50);
    if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_16MHZ) != CAN_OK) {
      return false;
    }
  }
  CAN.setMode(MCP_NORMAL);
  return true;
}

void createSessionFolder() {
  uint16_t highestSession = 0;

  File root = SD.open("/");
  if (root) {
    File entry = root.openNextFile();
    while (entry) {
      if (entry.isDirectory()) {
        const char *name = entry.name();
        const char *base = name;
        if (base[0] == '/') base++;

        if (strlen(base) == 7 && base[0] == 'L' && base[1] == 'O' && base[2] == 'G') {
          uint16_t num = (uint16_t)atoi(base + 3);
          if (num > highestSession) highestSession = num;
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

  if (SD.mkdir(folderPath))
    Serial.printf("[SD] Created session folder: %s\n", folderPath);
  else
    Serial.printf("[SD] WARNING: Could not create %s\n", folderPath);

  snprintf(logFilePath, sizeof(logFilePath), "%s/supra_log.csv", folderPath);
}

void sendNextCANRequest() {
  uint8_t requestPayload[3] = {REQ_PREFIX, parameterIDs[currentParamIndex], REQ_SUFFIX};

  uint8_t result = CAN.sendMsgBuf(CAN_ID_REQUEST, 0, 3, requestPayload);
  if (result == CAN_OK) {
    Serial.printf("[CAN TX] ID:0x%03X  Data: %02X %02X %02X\n",
                  CAN_ID_REQUEST, requestPayload[0], requestPayload[1], requestPayload[2]);
  } else {
    Serial.printf("[CAN TX] FAILED param 0x%02X (err %d)\n",
                  parameterIDs[currentParamIndex], result);
  }

  currentParamIndex = (currentParamIndex + 1) % NUM_PARAMETERS;
}

void ensureCSVHeader() {
  if (SD.exists(logFilePath)) {
    Serial.println(F("[SD] Existing log file found - appending."));
    return;
  }
  File f = SD.open(logFilePath, FILE_WRITE);
  if (f) {
    f.println(F("ID,Length,Byte0,Byte1,Byte2,Timestamp_ms"));
    f.close();
    Serial.println(F("[SD] New log file created with CSV header."));
  }
}

void logCANFrame(unsigned long id, uint8_t len, const uint8_t *buf) {
  uint8_t b0 = (len > 0) ? buf[0] : 0;
  uint8_t b1 = (len > 1) ? buf[1] : 0;
  uint8_t b2 = (len > 2) ? buf[2] : 0;

  Serial.printf("[%06lu] #%lu  ID:0x%03lX  Len:%d  Data: %02X %02X %02X\n",
                millis(), frameCount, id, len, b0, b1, b2);

  File f = SD.open(logFilePath, FILE_APPEND);
  if (f) {
    char csvLine[64];
    snprintf(csvLine, sizeof(csvLine), "0x%03lX,%d,%02X,%02X,%02X,%lu",
             id, len, b0, b1, b2, millis());
    f.println(csvLine);
    f.close();
  }
}
