#include <SPI.h>
#include <SD.h>
#include <mcp_can.h>
#include <MCUFRIEND_kbv.h>
#include <Adafruit_GFX.h>

/* ===================== TFT ===================== */
MCUFRIEND_kbv tft;

/* ===================== PINS ===================== */
#define SD_CS   10
#define CAN_CS  A5

MCP_CAN CAN(CAN_CS);
File logFile;

/* ===================== 8.3 FILE SYSTEM ===================== */
void openNewLogFile() {
  char filename[13];

  for (uint8_t i = 0; i < 200; i++) {
    snprintf(filename, sizeof(filename), "CANLG%03u.CSV", i);

    if (!SD.exists(filename)) {
      logFile = SD.open(filename, FILE_WRITE);

      if (!logFile) {
        tft.setCursor(10, 200);
        tft.print("Log open FAIL");
        while (1);
      }

      logFile.println("TIME_MS,CAN_ID,EXT,DLC,D0,D1,D2,D3,D4,D5,D6,D7");
      logFile.flush();

      tft.setCursor(10, 200);
      tft.print("LOG: ");
      tft.print(filename);
      return;
    }
  }

  tft.setCursor(10, 200);
  tft.print("No filenames");
  while (1);
}

/* ===================== DRAW STATIC UI ===================== */
void drawStaticUI() {
  tft.fillScreen(0x0000);
  tft.setTextColor(0xFFFF);
  tft.setTextSize(2);

  tft.setCursor(10, 10);
  tft.print("CAN LOGGER");

  tft.setCursor(10, 50);
  tft.print("ID:");

  tft.setCursor(10, 80);
  tft.print("DLC:");

  tft.setCursor(10, 110);
  tft.print("DATA:");

  tft.setCursor(10, 160);
  tft.print("TIME:");
}

/* ===================== SETUP ===================== */
void setup() {
  Serial.begin(115200);

  /* TFT INIT */
  uint16_t ID = tft.readID();
  tft.begin(ID);
  tft.setRotation(1);

  drawStaticUI();

  /* SD INIT */
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, LOW);

  if (!SD.begin(SD_CS)) {
    tft.setCursor(10, 200);
    tft.print("SD FAIL");
    while (1);
  }

  openNewLogFile();
  digitalWrite(SD_CS, HIGH);

  /* CAN INIT */
  tft.setCursor(150, 10);
  tft.print("INIT");

  while (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) != CAN_OK) {
    delay(500);
  }

  CAN.setMode(MCP_NORMAL);

  tft.setCursor(150, 10);
  tft.print("READY");
}

/* ===================== LOOP ===================== */
void loop() {

  if (CAN.checkReceive() == CAN_MSGAVAIL) {

    unsigned long rxId;
    byte len;
    byte buf[8];

    CAN.readMsgBuf(&rxId, &len, buf);
    unsigned long t = millis();

    /* ---------- SD LOG ---------- */
    digitalWrite(SD_CS, LOW);

    logFile.print(t);
    logFile.print(",");
    logFile.print(rxId, HEX);
    logFile.print(",");
    logFile.print(rxId > 0x7FF ? 1 : 0);
    logFile.print(",");
    logFile.print(len);

    for (int i = 0; i < 8; i++) {
      logFile.print(",");
      if (i < len) logFile.print(buf[i], HEX);
      else logFile.print("--");
    }

    logFile.println();
    logFile.flush();

    digitalWrite(SD_CS, HIGH);

    /* ---------- TFT LIVE DISPLAY ---------- */
    tft.setTextColor(0x07E0, 0x0000); // green text, black background

    tft.setCursor(70, 50);
    tft.print("        ");
    tft.setCursor(70, 50);
    tft.print(rxId, HEX);

    tft.setCursor(70, 80);
    tft.print("   ");
    tft.setCursor(70, 80);
    tft.print(len);

    tft.setCursor(70, 110);
    tft.print("                ");
    tft.setCursor(70, 110);
    for (int i = 0; i < len; i++) {
      tft.print(buf[i], HEX);
      tft.print(" ");
    }

    tft.setCursor(70, 160);
    tft.print("        ");
    tft.setCursor(70, 160);
    tft.print(t);

    Serial.print("ID: ");
    Serial.print(rxId, HEX);
    Serial.print("  Data: ");
    for (int i = 0; i < len; i++) {
      Serial.print(buf[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }
}
