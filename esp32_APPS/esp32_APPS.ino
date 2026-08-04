/*
  EV Ready-to-Drive controller
  Ported: Arduino (ATmega + "CAN.h") -> ESP32 DevKit V1 + MCP2515 SPI CAN module

  ============================= HARDWARE (per schematic) =============================
  MCP2515 module (no INT pin wired -> firmware polls instead of using an interrupt):
    CS   -> D5
    SCK  -> D18
    MOSI -> D23
    MISO -> D19
  These are ESP32's default VSPI pins, so no SPI.begin() remapping is needed.

  NOTE: this sketch needs the "autowp-mcp2515" 
  Sensors / IO:
    APPS1        -> D32 (ADC1_CH4)
    APPS2        -> D33 (ADC1_CH5)
    BRAKE        -> D34 (ADC1_CH6)
    SDC sense    -> D21
    BUTTON       -> D14, active LOW (button shorts D14 to GND when pressed)
    BUZZER       -> D4  -> Q6 gate -> buzzer
    BRAKE LIGHT  -> D13 -> Q5 gate -> brake light
    K1 (Q1)      -> D15 -> dry contact enabling HV relay's Aux connection
    K2 (Q2)      -> D2  -> sends 12V to motor controller, AFTER RTDS completes

  ============================= NEW SEQUENCE =============================
  1. Driver presses D14 button (active LOW) once - this LATCHES D15.
  2. D15 (RELAY_PIN) latches HIGH on that single press -> Q1 -> K1 dry
     contact closes, enabling the HV relay's Aux connection (this is what
     lets precharge happen). The driver does NOT need to keep holding the
     button - D15 stays HIGH through the whole precharge wait.
  3. Firmware sits in RTD_PRECHARGE_WAIT until it sees CAN ID 0x10 with
     byte[0] != 0 (precharge complete). Only a fault (SDC open) aborts the
     wait and unlatches D15 - the driver does not need to hold the button
     through this wait.
  4. Once precharge is confirmed, RTDS does NOT auto-trigger just because
     brake is held. The driver must press the button AGAIN - a fresh
     press, distinct from the original latching press, detected as a
     rising edge - while the brake is held, to actually start RTDS.
  5. Only then does the buzzer (D4 -> Q6) sound, for EXACTLY 2.5 seconds -
     no more, no less, regardless of anything else changing mid-chime.
  6. Once those 2.5s are up (RTDS "done"), D2 (K2/Q2) goes HIGH -> 12V is
     sent to the motor controller so it accepts torque commands over CAN.
  7. Torque is only ever computed/sent while D2 is HIGH (rtdState ==
     RTD_READY). Any SDC/pedal fault drops D2 low, unlatches D15/K1, and
     the whole sequence (including precharge confirmation and both button
     presses) has to happen again from scratch.
  ==========================================================================
*/

#include <SPI.h>
#include <mcp2515.h>

// ---------------- Pin map (ESP32 DevKit V1, per schematic) ----------------
const int APPS1_PIN = 32; // ADC1_CH4
const int APPS2_PIN = 33; // ADC1_CH5
const int BRAKE_PIN = 34; // ADC1_CH6

const int SDC_PIN       = 21; // shutdown circuit sense
const int BUTTON_PIN    = 14; // active LOW, shorts to GND when pressed
const int BUZZER_PIN    = 4;  // -> Q6 -> buzzer
const int BRAKE_LIGHT   = 13; // -> Q5 -> brake light
const int RELAY_PIN     = 15; // -> Q1 -> K1 (HV relay Aux enable)
const int MC_ENABLE_PIN = 2;  // -> Q2 -> K2 (12V to motor controller)

const int MCP_CS_PIN = 5; // SCK/MOSI/MISO are the default VSPI pins (18/23/19)

// ---------------- CAN IDs ----------------
#define TORQUE_TX_ID        0x201
#define PRECHARGE_STATUS_ID 0x10

// ---------------- Sensor calibration ----------------

#define APPS1_SAFE_MIN 400
#define APPS1_SAFE_MAX 2400
#define APPS2_SAFE_MIN 320
#define APPS2_SAFE_MAX 2200

#define APPS1_MIN 880
#define APPS1_MAX 2340
#define APPS2_MIN 720
#define APPS2_MAX 2100

#define BRAKE_MIN 1200
#define BRAKE_MAX 2000

// ---------------- RTDS timing ----------------
#define RTDS_DURATION_MS 2500UL // exactly 2.5s, no more, no less

// ---------------- Ready-to-drive state machine ----------------
enum RtdState {
  RTD_IDLE,            // waiting for driver to press the button
  RTD_PRECHARGE_WAIT,  // D15/K1 fired, waiting for CAN precharge-complete
  RTD_ACTIVE,          // RTDS buzzer sounding, exactly 2.5s
  RTD_READY            // D2/K2 high, torque allowed to motor controller
};
RtdState rtdState = RTD_IDLE;

bool implausibility = false;
unsigned long implausibilityStart = 0;

unsigned long rtdsStartTime = 0;
bool precharge_done = false;
bool relayLatched = false; // true once D15/K1 has been triggered by a button
                            // press; stays true through the precharge wait
                            // even if the driver lets go, cleared on fault
bool prevButtonPressed = false; // for detecting a fresh (new) button press,
                                 // separate from the initial latching press

int torque_percent = 0;

unsigned long lastCANrxTime = 0;
bool canTimedOut = true;

int bootBurstCount = 0;

MCP2515 mcp2515(MCP_CS_PIN);

void sendCAN(uint32_t id, const uint8_t *data, uint8_t len) {
  struct can_frame frame;
  frame.can_id = id;
  frame.can_dlc = len;
  for (uint8_t i = 0; i < len; i++) frame.data[i] = data[i];
  mcp2515.sendMessage(&frame);
}

void setup() {
  Serial.begin(115200);

  analogReadResolution(12); // native ESP32 range: 0-4095

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(MC_ENABLE_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(SDC_PIN, INPUT_PULLUP);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // active LOW: pressed = shorted to GND
  pinMode(BRAKE_LIGHT, OUTPUT);

  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(MC_ENABLE_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(BRAKE_LIGHT, LOW);

  pinMode(APPS1_PIN, INPUT);
  pinMode(APPS2_PIN, INPUT);
  pinMode(BRAKE_PIN, INPUT);

  SPI.begin(); // default VSPI pins: SCK=18, MISO=19, MOSI=23
  mcp2515.reset();
  mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ); // change MCP_8MHZ if your module uses 16MHz
  mcp2515.setNormalMode();

  Serial.println("[R2D] DISABLED - waiting for button");
}

void loop() {
  // ---- boot CAN burst (kept from original) ----
  if (bootBurstCount < 15) {
    delay(100);
    uint8_t data[3] = {0x3D, 0xEB, 0x64};
    sendCAN(TORQUE_TX_ID, data, 3);
    bootBurstCount++;
    Serial.println("CAN MESSAGE SENT");
  }

  // ---- CAN receive (polled, no INT pin wired) ----
  struct can_frame rx_frame;
  while (mcp2515.readMessage(&rx_frame) == MCP2515::ERROR_OK) {
    lastCANrxTime = millis();
    canTimedOut = false;

    if (rx_frame.can_id == PRECHARGE_STATUS_ID && rx_frame.can_dlc >= 1) {
      precharge_done = (rx_frame.data[0] != 0);
    }
  }
  if (millis() - lastCANrxTime > 500) canTimedOut = true;

  // ---- Digital / analog inputs ----
  bool sdc_ok = (digitalRead(SDC_PIN) == LOW);
  bool buttonPressed = (digitalRead(BUTTON_PIN) == LOW); // active LOW
  bool buttonRisingEdge = buttonPressed && !prevButtonPressed; // fresh press this loop

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

  bool apps_invalid = apps1_out || apps2_out; // NOTE: computed but not wired into
                                               // a fault below - same as your
                                               // original code. Say the word if
                                               // you want this to actually cut
                                               // torque too.
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

  bool pedalCriticalFault = diff_fault || brakeOverride;
  bool criticalFault = !sdc_ok;

  // ---- D15 / K1: latched by a button press, held until fault or full reset ----
  digitalWrite(RELAY_PIN, relayLatched ? HIGH : LOW);

  // ---- Ready-to-drive state machine ----
  switch (rtdState) {

    case RTD_IDLE:
      digitalWrite(MC_ENABLE_PIN, LOW);
      digitalWrite(BUZZER_PIN, LOW);
      if (buttonPressed && sdc_ok && !criticalFault) {
        relayLatched = true; // one press latches D15/K1 on
        rtdState = RTD_PRECHARGE_WAIT;
        Serial.println("[R2D] Button pressed -> K1 latched, waiting on precharge");
      }
      break;

    case RTD_PRECHARGE_WAIT:
      // Button no longer needs to be held to stay in this state - the
      // relay is latched. But once precharge is confirmed, RTDS/R2D does
      // NOT auto-trigger just because brake is held: the driver must press
      // the button again (a fresh press, not just holding it from the
      // original latch press) together with the brake.
      if (!sdc_ok || criticalFault) {
        relayLatched = false;
        rtdState = RTD_IDLE; // fault -> abort, unlatch, start over
        break;
      }
      if (precharge_done && buttonRisingEdge && brake_raw > 1140) { // 285 * 4, scaled to 12-bit
        rtdState = RTD_ACTIVE;
        rtdsStartTime = millis();
        digitalWrite(BUZZER_PIN, HIGH);
        Serial.println("[R2D] Precharge confirmed + button re-pressed w/ brake -> RTDS sounding");
      }
      break;

    case RTD_ACTIVE:
      if (!sdc_ok || criticalFault) {
        digitalWrite(BUZZER_PIN, LOW);
        relayLatched = false;
        rtdState = RTD_IDLE;
        break;
      }
      // Buzzer runs the full, fixed 2.5s regardless of anything else -
      // no early cutoff, no extension.
      if (millis() - rtdsStartTime >= RTDS_DURATION_MS) {
        digitalWrite(BUZZER_PIN, LOW);
        digitalWrite(MC_ENABLE_PIN, HIGH); // D2/K2 HIGH -> MC gets 12V, accepts CAN torque
        rtdState = RTD_READY;
        Serial.println("[R2D] RTDS done - K2 energized, torque enabled");
      }
      break;

    case RTD_READY:
      if (!sdc_ok || criticalFault || pedalCriticalFault) {
        digitalWrite(MC_ENABLE_PIN, LOW); // D2/K2 LOW -> MC stops accepting torque
        relayLatched = false; // full reset: driver must press button again
        rtdState = RTD_IDLE;
        precharge_done = false; // require re-confirmation next cycle
        Serial.println("[R2D] Fault -> torque disabled, restart sequence required");
      }
      break;
  }

  bool r2d_enabled = (rtdState == RTD_READY);

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
    sendCAN(TORQUE_TX_ID, txdata, 3);
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
    Serial.print(" | SDC="); Serial.print(sdc_ok ? "OK" : "OPEN");
    Serial.print(" | PRECHG="); Serial.print(precharge_done ? "DONE" : "WAIT");
    Serial.print(" | BRAKE="); Serial.print(brake_raw);
    Serial.print(" | APPS1="); Serial.print(apps1_raw);
    Serial.print(" | APPS2="); Serial.print(apps2_raw);
    Serial.print(" | TORQUE%="); Serial.print(torque_percent);
    Serial.println();
  }

  prevButtonPressed = buttonPressed; // must run every loop, after all edge checks
}
