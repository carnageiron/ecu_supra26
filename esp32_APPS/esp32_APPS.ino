#include <CAN.h>

// ---------------- Pin map (ORIGINAL, unchanged) ----------------
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

// ---------------- Sensor calibration (original 10-bit AVR ADC, 0-1023) ----------------
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
#define RTDS_DURATION_MS 2500UL // exactly 2.5s, no more, no less

// ---------------- Ready-to-drive state machine ----------------
enum RtdState {
  RTD_IDLE,            
  RTD_PRECHARGE_WAIT,  
  RTD_ACTIVE,          
  RTD_READY            
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

int i = 0; // boot CAN burst counter

void setup() {
  Serial.begin(9600);
  CAN.setClockFrequency(8E6);

  pinMode(FRG_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(SDC_PIN, INPUT_PULLUP);
  pinMode(BUTTON_PIN, INPUT);
  pinMode(BRAKE_LIGHT, OUTPUT);

  digitalWrite(FRG_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(A3, INPUT);
  pinMode(A6, INPUT);
  pinMode(A7, INPUT);

  if (!CAN.begin(500E3)) {
    Serial.println("CAN FAILED");
    while (1);
  }

  Serial.println("[R2D] DISABLED - waiting for button");
}

void loop() {

  // ---- boot CAN burst (kept from original) ----
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
    canTimedOut = false;

    long rxID = CAN.packetId();
    if (rxID == PRECHARGE_STATUS_ID && CAN.available()) {
      uint8_t b0 = CAN.read();
      precharge_done = (b0 != 0);
    }
    while (CAN.available()) CAN.read();
  }

  if (millis() - lastCANrxTime > 500) {
    canTimedOut = true;
  }

  // ---- Digital / analog inputs ----
  bool sdc_ok = (digitalRead(SDC_PIN) == LOW);
  bool buttonPressed = (digitalRead(BUTTON_PIN) == HIGH);
  bool buttonRisingEdge = buttonPressed && !prevButtonPressed; // fresh press this loop

  if (!sdc_ok) digitalWrite(FRG_PIN, LOW);

  int apps1_raw = analogRead(APPS1_PIN);
  int apps2_raw = analogRead(APPS2_PIN);
  int brake_raw = analogRead(BRAKE_PIN);

  bool apps1_out = (apps1_raw < APPS1_SAFE_MIN || apps1_raw > APPS1_SAFE_MAX);
  bool apps2_out = (apps2_raw < APPS2_SAFE_MIN || apps2_raw > APPS2_SAFE_MAX);

  int apps1_clamped = constrain(apps1_raw, APPS1_MIN, APPS1_MAX);
  int apps2_clamped = constrain(apps2_raw, APPS2_MIN, APPS2_MAX);

  int apps1_percent = map(apps1_clamped, APPS1_MIN, APPS1_MAX, 0, 100);
  int apps2_percent = map(apps2_clamped, APPS2_MIN, APPS2_MAX, 0, 100);

  int brake_percent = 0;
  if (brake_raw >= BRAKE_MIN && brake_raw <= BRAKE_MAX)
    brake_percent = map(brake_raw, BRAKE_MIN, BRAKE_MAX, 0, 100);

  int avg_apps = (apps1_percent + apps2_percent) / 2;
  int diff = abs(apps1_percent - apps2_percent);
  bool diff_fault = (diff > 10);

  bool brake_pressed = brake_percent > 30;
  bool accel_pressed = avg_apps > 5;
  bool brakeOverride = brake_pressed && accel_pressed;

  bool apps_invalid = apps1_out || apps2_out; // NOTE: computed but not wired into a fault below - same as original code.
  bool apps_fault = diff_fault;

  if (!implausibility) {
    if (apps_fault) {
      if (!implausibilityStart) implausibilityStart = millis();
      else if (millis() - implausibilityStart >= 100) implausibility = true;
    } else {
      implausibilityStart = 0;
    }
  } else {
    if (avg_apps == 0 && brake_percent == 0) {
      implausibility = false;
      implausibilityStart = 0;
    }
  }

  // implausibility now folded into the critical pedal fault, so a tripped
  // implausibility latch also forces a full restart, not just torque = 0
  bool pedalCriticalFault = diff_fault || implausibility;
  bool criticalFault = !sdc_ok;

  if (pedalCriticalFault) {
    digitalWrite(FRG_PIN, LOW);
  }

  // ---- Ready-to-drive state machine ----
  switch (rtdState) {

    case RTD_IDLE:
      digitalWrite(FRG_PIN, LOW);
      digitalWrite(BUZZER_PIN, LOW);
      if (buttonPressed && sdc_ok && !criticalFault) {
        rtdState = RTD_PRECHARGE_WAIT;
        Serial.println("[R2D] Button pressed -> waiting on precharge");
      }
      break;

    case RTD_PRECHARGE_WAIT:
      // Button does not need to be held here. Once precharge is confirmed,
      // RTDS does NOT auto-trigger just because brake is held - the driver
      // must press the button again (a fresh press) together with brake.
      if (!sdc_ok || criticalFault) {
        rtdState = RTD_IDLE; // fault -> abort, start over
        break;
      }
      if (precharge_done && buttonRisingEdge && brake_raw > 285) {
        rtdState = RTD_ACTIVE;
        rtdsStartTime = millis();
        digitalWrite(BUZZER_PIN, HIGH);
        Serial.println("[R2D] Precharge confirmed + button re-pressed w/ brake -> RTDS sounding");
      }
      break;

    case RTD_ACTIVE:
      if (!sdc_ok || criticalFault) {
        digitalWrite(BUZZER_PIN, LOW);
        rtdState = RTD_IDLE;
        break;
      }

      if (millis() - rtdsStartTime >= RTDS_DURATION_MS) {
        digitalWrite(BUZZER_PIN, LOW);
        if (sdc_ok && !criticalFault) {
          digitalWrite(FRG_PIN, HIGH); // R2D engaged, torque enabled
        }
        rtdState = RTD_READY;
        Serial.println("[R2D] RTDS done - torque enabled");
      }
      break;

    case RTD_READY:
      if (!sdc_ok || criticalFault || pedalCriticalFault) {
        digitalWrite(FRG_PIN, LOW);
        rtdState = RTD_IDLE;
        precharge_done = false; // require re-confirmation next cycle
        Serial.println("[R2D] Fault -> torque disabled, restart sequence required");
      }
      break;
  }

  bool r2d_enabled = digitalRead(FRG_PIN);

  if (!criticalFault && r2d_enabled) {
    torque_percent = avg_apps;
  } else {
    torque_percent = 0;
  }

  bool brake_light_on = brake_percent > 30;
  digitalWrite(BRAKE_LIGHT, brake_light_on ? HIGH : LOW);

  int torque_cmd = map(torque_percent, 0, 100, 0, 32767);

  uint8_t txdata[3] = {
    0x90,
    (uint8_t)(torque_cmd & 0xFF),
    (uint8_t)((torque_cmd >> 8) & 0xFF)
  };

  if (!criticalFault) {
    CAN.beginPacket(0x201);
    CAN.write(txdata, 3);
    CAN.endPacket();
  }

  delay(10);

  static unsigned long lastStatusPrint = 0;
  if (millis() - lastStatusPrint >= 200) {
    lastStatusPrint = millis();

    Serial.print("[STATE] ");
    switch (rtdState) {
      case RTD_IDLE:           Serial.print("IDLE"); break;
      case RTD_PRECHARGE_WAIT: Serial.print("PRECHARGE_WAIT"); break;
      case RTD_ACTIVE:         Serial.print("RTDS_ACTIVE"); break;
      case RTD_READY:          Serial.print("READY"); break;
    }

    Serial.print(" | SDC=");
    Serial.print(sdc_ok ? "OK" : "OPEN");

    Serial.print(" | PRECHG=");
    Serial.print(precharge_done ? "DONE" : "WAIT");

    Serial.print("  BRAKE RAW=");
    Serial.print(brake_raw);

    Serial.print("  APPS_1=");
    Serial.print(apps1_raw);

    Serial.print("  APPS_2=");
    Serial.print(apps2_raw);

    Serial.print("  TORQUE%=");
    Serial.print(torque_percent);

    Serial.println();
  }

  prevButtonPressed = buttonPressed; // must run every loop, after all edge checks
}
