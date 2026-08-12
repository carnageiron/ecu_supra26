#include <CAN.h>

// ---------------- Pin map (unchanged) ----------------
const int APPS1_PIN = A7;
const int APPS2_PIN = A6;
const int BRAKE_PIN  = A3;

const int FRG_PIN     = 4;
const int BUTTON_PIN  = 7;
const int BUZZER_PIN  = 5;
const int SDC_PIN     = 8;
const int BRAKE_LIGHT = 3;

// ---------------- CAN IDs ----------------
#define TORQUE_TX_ID        0x201
#define PRECHARGE_STATUS_ID 0x10

// ---------------- Sensor calibration (unchanged) ----------------
#define APPS1_SAFE_MIN 100
#define APPS1_SAFE_MAX 600
#define APPS2_SAFE_MIN 80
#define APPS2_SAFE_MAX 550

#define APPS1_MIN 220
#define APPS1_MAX 585
#define APPS2_MIN 180
#define APPS2_MAX 525

#define BRAKE_MIN 300
#define BRAKE_MAX 500

// ---------------- RTDS timing ----------------
#define RTDS_DURATION_MS 2500UL

// -----------------------------------------------------------------------
// RTD sequence (REVISED):
//
//  RTD_IDLE          – waiting for driver input
//  RTD_ACTIVE        – RTDS sounding (2.5 s); FRG still LOW
//  RTD_PRECHARGE_WAIT– FRG goes HIGH → HV contactor latches → precharge
//                      running; torque still 0
//  RTD_READY         – precharge confirmed; torque enabled
//
// -----------------------------------------------------------------------
enum RtdState {
  RTD_IDLE,
  RTD_ACTIVE,           // RTDS buzzing; contactor not yet closed
  RTD_PRECHARGE_WAIT,   // FRG HIGH; waiting for precharge_done over CAN
  RTD_READY             // full drive
};
RtdState rtdState = RTD_IDLE;

bool implausibility = false;
unsigned long implausibilityStart = 0;

unsigned long rtdsStartTime = 0;
bool precharge_done = false;
bool prevButtonPressed = false;

int torque_percent = 0;

unsigned long lastCANrxTime = 0;
bool canTimedOut = true;

int i = 0;

// -----------------------------------------------------------------------
void setup() {
  Serial.begin(9600);
  CAN.setClockFrequency(8E6);

  pinMode(FRG_PIN,     OUTPUT);
  pinMode(BUZZER_PIN,  OUTPUT);
  pinMode(SDC_PIN,     INPUT_PULLUP);
  pinMode(BUTTON_PIN,  INPUT);
  pinMode(BRAKE_LIGHT, OUTPUT);
  pinMode(A3, INPUT);
  pinMode(A6, INPUT);
  pinMode(A7, INPUT);

  digitalWrite(FRG_PIN,    LOW);
  digitalWrite(BUZZER_PIN, LOW);

  if (!CAN.begin(500E3)) {
    Serial.println("CAN FAILED");
    while (1);
  }

  Serial.println("[R2D] DISABLED - waiting for button + brake");
}

// -----------------------------------------------------------------------
void loop() {

  // ---- boot CAN burst (unchanged) ----
  if (i < 15) {
    delay(100);
    CAN.beginPacket(0x201);
    CAN.write(0x3D);
    CAN.write(0xEB);
    CAN.write(0x64);
    CAN.endPacket();
    i++;
    Serial.println("CAN MESSAGE SENT");
  }

  // ---- CAN receive ----
  int packetSize = CAN.parsePacket();
  if (packetSize) {
    lastCANrxTime = millis();
    canTimedOut   = false;

    long rxID = CAN.packetId();
    if (rxID == PRECHARGE_STATUS_ID && CAN.available()) {
      uint8_t b0 = CAN.read();
      precharge_done = (b0 != 0);
    }
    while (CAN.available()) CAN.read();
  }
  if (millis() - lastCANrxTime > 500) canTimedOut = true;

  // ---- Digital / analog inputs ----
  bool sdc_ok         = (digitalRead(SDC_PIN)    == LOW);
  bool buttonPressed  = (digitalRead(BUTTON_PIN) == HIGH);
  bool buttonRisingEdge = buttonPressed && !prevButtonPressed;

  int apps1_raw = analogRead(APPS1_PIN);
  int apps2_raw = analogRead(APPS2_PIN);
  int brake_raw = analogRead(BRAKE_PIN);

  // Range-check faults
  bool apps1_out = (apps1_raw < APPS1_SAFE_MIN || apps1_raw > APPS1_SAFE_MAX);
  bool apps2_out = (apps2_raw < APPS2_SAFE_MIN || apps2_raw > APPS2_SAFE_MAX);

  // Percent calculation
  int apps1_percent = map(constrain(apps1_raw, APPS1_MIN, APPS1_MAX), APPS1_MIN, APPS1_MAX, 0, 100);
  int apps2_percent = map(constrain(apps2_raw, APPS2_MIN, APPS2_MAX), APPS2_MIN, APPS2_MAX, 0, 100);

  int brake_percent = 0;
  if (brake_raw >= BRAKE_MIN && brake_raw <= BRAKE_MAX)
    brake_percent = map(brake_raw, BRAKE_MIN, BRAKE_MAX, 0, 100);

  int avg_apps  = (apps1_percent + apps2_percent) / 2;
  int diff      = abs(apps1_percent - apps2_percent);
  bool diff_fault   = (diff > 10);
  bool apps_invalid = apps1_out || apps2_out;
  bool apps_fault   = diff_fault || apps_invalid;

  // ---- APPS implausibility latch (T.4.2 / FB rules) ----
  if (!implausibility) {
    if (apps_fault) {
      if (!implausibilityStart) implausibilityStart = millis();
      else if (millis() - implausibilityStart >= 100) implausibility = true;
    } else {
      implausibilityStart = 0;
    }
  } else {
    if (avg_apps == 0 && brake_percent == 0) {
      implausibility      = false;
      implausibilityStart = 0;
    }
  }

  bool pedalCriticalFault = diff_fault || implausibility;
  bool criticalFault      = !sdc_ok;

  if (pedalCriticalFault) digitalWrite(FRG_PIN, LOW);
  if (!sdc_ok)            digitalWrite(FRG_PIN, LOW);

  // -----------------------------------------------------------------------
  // Ready-to-drive state machine  (REVISED order)
  // -----------------------------------------------------------------------
  switch (rtdState) {

    // ------------------------------------------------------------------
    case RTD_IDLE:
      // Ensure outputs are safe
      digitalWrite(FRG_PIN,    LOW);
      digitalWrite(BUZZER_PIN, LOW);

      // Rising-edge button press + brake pedal depressed + SDC closed
      if (buttonRisingEdge && sdc_ok && !criticalFault && brake_raw > 285) {
        rtdState      = RTD_ACTIVE;
        rtdsStartTime = millis();
        digitalWrite(BUZZER_PIN, HIGH);
        Serial.println("[R2D] Button + brake confirmed -> RTDS sounding");
      }
      break;

    // ------------------------------------------------------------------
    // RTDS buzzing for 2.5 s; FRG is still LOW (contactor still open)
    // Any fault here aborts cleanly before any HV is involved.
    case RTD_ACTIVE:
      if (!sdc_ok || criticalFault) {
        digitalWrite(BUZZER_PIN, LOW);
        rtdState = RTD_IDLE;
        Serial.println("[R2D] Fault during RTDS -> aborted");
        break;
      }

      if (millis() - rtdsStartTime >= RTDS_DURATION_MS) {
        digitalWrite(BUZZER_PIN, LOW);
        // FRG HIGH → HV DC contactor relay latches → precharge begins
        digitalWrite(FRG_PIN, HIGH);
        rtdState = RTD_PRECHARGE_WAIT;
        Serial.println("[R2D] RTDS done - FRG HIGH, HV contactor closed, waiting for precharge");
      }
      break;

    // ------------------------------------------------------------------
    // Contactor is closed; precharging. Torque output remains 0 until
    // precharge_done is confirmed over CAN.
    case RTD_PRECHARGE_WAIT:
      if (!sdc_ok || criticalFault) {
        digitalWrite(FRG_PIN, LOW);   // open contactor on fault
        precharge_done = false;
        rtdState = RTD_IDLE;
        Serial.println("[R2D] Fault during precharge -> FRG LOW, restart required");
        break;
      }

      if (precharge_done) {
        rtdState = RTD_READY;
        Serial.println("[R2D] Precharge complete - torque enabled");
      }
      break;

    // ------------------------------------------------------------------
    // Full drive; any safety fault kills torque and opens contactor.
    case RTD_READY:
      if (!sdc_ok || criticalFault || pedalCriticalFault) {
        digitalWrite(FRG_PIN, LOW);
        precharge_done = false;
        rtdState = RTD_IDLE;
        Serial.println("[R2D] Fault -> FRG LOW, torque disabled, restart required");
      }
      break;
  }

  // ---- Torque command ----
  // Only non-zero when fully ready AND FRG confirmed HIGH AND no faults
  bool r2d_active = (rtdState == RTD_READY) && digitalRead(FRG_PIN);

  torque_percent = (!criticalFault && !pedalCriticalFault && r2d_active)
                   ? avg_apps
                   : 0;

  // ---- Brake light ----
  digitalWrite(BRAKE_LIGHT, (brake_percent > 30) ? HIGH : LOW);

  // ---- CAN TX ----
  int torque_cmd = map(torque_percent, 0, 100, 0, 32767);
  uint8_t txdata[3] = {
    0x90,
    (uint8_t)(torque_cmd & 0xFF),
    (uint8_t)((torque_cmd >> 8) & 0xFF)
  };

  if (!criticalFault) {
    CAN.beginPacket(TORQUE_TX_ID);
    CAN.write(txdata, 3);
    CAN.endPacket();
  }

  delay(10);

  // ---- Serial status @ 5 Hz ----
  static unsigned long lastStatusPrint = 0;
  if (millis() - lastStatusPrint >= 200) {
    lastStatusPrint = millis();

    Serial.print("[STATE] ");
    switch (rtdState) {
      case RTD_IDLE:           Serial.print("IDLE");           break;
      case RTD_ACTIVE:         Serial.print("RTDS_ACTIVE");    break;
      case RTD_PRECHARGE_WAIT: Serial.print("PRECHARGE_WAIT"); break;
      case RTD_READY:          Serial.print("READY");          break;
    }
    Serial.print(" | SDC=");       Serial.print(sdc_ok        ? "OK"   : "OPEN");
    Serial.print(" | PRECHG=");    Serial.print(precharge_done ? "DONE" : "WAIT");
    Serial.print(" | BRAKE_RAW="); Serial.print(brake_raw);
    Serial.print(" | APPS1=");     Serial.print(apps1_raw);
    Serial.print(" | APPS2=");     Serial.print(apps2_raw);
    Serial.print(" | TORQUE%=");   Serial.print(torque_percent);
    Serial.println();
  }

  prevButtonPressed = buttonPressed;
}