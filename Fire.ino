#include <Arduino_RouterBridge.h>
int red_pin = 4;
int green_pin = 2;
int buzzer_pin = 10; 
int gas = A0;
int flame = 7;

int flame_reading;
int gas_reading;      // smoothed value — this is what drives decisions and the dashboard
int gas_reading_raw;  // unsmoothed, straight from the sensor — for debug comparison only
int gas_threshold = 500;
float smoothedGas = 0;
bool smoothedGasInitialized = false;
const float GAS_SMOOTHING_ALPHA = 0.15; // lower = smoother but slower to react; higher = more responsive but noisier

bool danger = false;

// ---------- Manual buzzer override (dashboard Test/Silence buttons) ----------
bool manualOverride = false;
bool manualBuzzerState = false;
unsigned long overrideStartTime = 0;
const unsigned long OVERRIDE_DURATION_MS = 5000;

// ---------- Buzzer type ----------
bool ACTIVE_BUZZER = true;

// ---------- Siren state (passive buzzer: pitch sweep) ----------
int sirenFreq = 800;
int sirenFreqStep = 40;
const int SIREN_FREQ_MIN = 800;
const int SIREN_FREQ_MAX = 1800;
unsigned long lastSirenStepTime = 0;
const unsigned long SIREN_STEP_INTERVAL_MS = 20;

// ---------- Siren state (active buzzer: pulse-rate wail) ----------
unsigned long lastPulseToggleTime = 0;
bool pulseOn = false;
int pulseIntervalMs = 60;
int pulseIntervalStep = 4;
const int PULSE_INTERVAL_MIN = 60;
const int PULSE_INTERVAL_MAX = 220;
bool buzzerWasOn = false;

// ---------- Red LED blink state ----------
unsigned long lastLedToggleTime = 0;
bool ledBlinkState = false;
const unsigned long LED_BLINK_INTERVAL_MS = 300;

unsigned long lastDebugPrint = 0;

// ===== Functions exposed to Python via the Bridge =====

int get_gas_value() {
  return gas_reading; // smoothed value, matches what drives the LED/buzzer decision
}

int get_flame_state() {
  return (digitalRead(flame) == HIGH) ? 1 : 0;
}

int get_danger_state() {
  return danger ? 1 : 0;
}

void set_gas_threshold(int newLimit) {
  gas_threshold = newLimit;
}

void set_buzzer(int state) {
  manualOverride = true;
  manualBuzzerState = (state != 0);
  overrideStartTime = millis();
}

void setup() {
  pinMode(red_pin, OUTPUT);
  pinMode(green_pin, OUTPUT);
  pinMode(buzzer_pin, OUTPUT);
  pinMode(flame, INPUT_PULLDOWN); // safe default = "no flame" if nothing's wired
                                   // (use plain INPUT if your board/core lacks PULLDOWN)
  pinMode(A0, INPUT);

  digitalWrite(green_pin, HIGH);
  digitalWrite(red_pin, LOW);
  digitalWrite(buzzer_pin, LOW);

  Monitor.begin();
  Bridge.begin();

  Bridge.provide("get_gas_value", get_gas_value);
  Bridge.provide("get_flame_state", get_flame_state);
  Bridge.provide("get_danger_state", get_danger_state);
  Bridge.provide("set_gas_threshold", set_gas_threshold);
  Bridge.provide("set_buzzer", set_buzzer);

  delay(20000); // MQ5 warm-up
}

void loop() {
  Bridge.update();

  flame_reading = digitalRead(flame);
  gas_reading_raw = analogRead(gas);

  if (!smoothedGasInitialized) {
    smoothedGas = gas_reading_raw;
    smoothedGasInitialized = true;
  } else {
    smoothedGas = GAS_SMOOTHING_ALPHA * gas_reading_raw + (1.0 - GAS_SMOOTHING_ALPHA) * smoothedGas;
  }
  gas_reading = (int)round(smoothedGas);

  danger = (flame_reading == HIGH || gas_reading >= gas_threshold);

  // ---------- Red LED: blink while in danger, solid green when safe ----------
  if (danger) {
    if (millis() - lastLedToggleTime >= LED_BLINK_INTERVAL_MS) {
      lastLedToggleTime = millis();
      ledBlinkState = !ledBlinkState;
      digitalWrite(red_pin, ledBlinkState ? HIGH : LOW);
    }
    digitalWrite(green_pin, LOW);
  } else {
    digitalWrite(red_pin, LOW);
    digitalWrite(green_pin, HIGH);
    ledBlinkState = false;
  }

  // ---------- Buzzer: manual override wins briefly, else follows real danger ----------
  bool buzzerOn;
  if (manualOverride && (millis() - overrideStartTime < OVERRIDE_DURATION_MS)) {
    buzzerOn = manualBuzzerState;
  } else {
    manualOverride = false;
    buzzerOn = danger;
  }

  if (buzzerOn) {
    if (!buzzerWasOn) {
      sirenFreq = SIREN_FREQ_MIN;
      sirenFreqStep = abs(sirenFreqStep);
      pulseIntervalMs = PULSE_INTERVAL_MIN;
      pulseIntervalStep = abs(pulseIntervalStep);
    }

    if (ACTIVE_BUZZER) {
      if (millis() - lastPulseToggleTime >= (unsigned long)pulseIntervalMs) {
        lastPulseToggleTime = millis();
        pulseOn = !pulseOn;
        digitalWrite(buzzer_pin, pulseOn ? HIGH : LOW);

        pulseIntervalMs += pulseIntervalStep;
        if (pulseIntervalMs >= PULSE_INTERVAL_MAX || pulseIntervalMs <= PULSE_INTERVAL_MIN) {
          pulseIntervalStep = -pulseIntervalStep;
        }
      }
    } else {
      if (millis() - lastSirenStepTime >= SIREN_STEP_INTERVAL_MS) {
        lastSirenStepTime = millis();
        tone(buzzer_pin, sirenFreq);

        sirenFreq += sirenFreqStep;
        if (sirenFreq >= SIREN_FREQ_MAX || sirenFreq <= SIREN_FREQ_MIN) {
          sirenFreqStep = -sirenFreqStep;
        }
      }
    }
  } else {
    if (buzzerWasOn) {
      noTone(buzzer_pin);
      digitalWrite(buzzer_pin, LOW);
    }
  }
  buzzerWasOn = buzzerOn;

  if (millis() - lastDebugPrint > 1000) {
    lastDebugPrint = millis();
    Monitor.print("Flame: ");
    Monitor.print(flame_reading == HIGH ? "HIGH (detected)" : "LOW (clear)");
    Monitor.print("  | Gas (smoothed): ");
    Monitor.print(gas_reading);
    Monitor.print("  | Gas (raw): ");
    Monitor.print(gas_reading_raw);
    Monitor.print("  | Status: ");
    Monitor.println(danger ? "DANGER" : "SAFE");
  }
}
