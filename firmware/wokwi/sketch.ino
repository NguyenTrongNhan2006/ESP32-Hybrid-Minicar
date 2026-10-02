#include <Arduino.h>
#include <ESP32Servo.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// Keep the user's ESP32-WROOM / Wokwi GPIO assignments.
constexpr uint8_t PIN_STEER = 18;
constexpr uint8_t PIN_GAS = 19;
constexpr uint8_t PIN_ESC = 23;          // Logic Analyzer D0
constexpr uint8_t PIN_RCEXL_TEST = 26;   // Logic Analyzer D1, TEST ONLY
constexpr uint8_t PIN_ESTOP = 27;        // INPUT_PULLUP; button to GND
constexpr uint8_t PIN_BATTERY = 34;      // ADC1; potentiometer SIG
constexpr uint8_t LED_ARMED = 25;        // green
constexpr uint8_t LED_FAILSAFE = 32;     // red (also STOPPED)
constexpr uint8_t LED_RUNNING = 33;      // yellow

constexpr uint32_t TIMEOUT_MS = 5000;
constexpr uint32_t BATTERY_SAMPLE_MS = 20;
constexpr int ESC_STOP_US = 1000;
constexpr int ESC_MAX_TEST_US = 1600;    // Preserve original test limit.
constexpr int RCEXL_STOP_TEST_US = 1000;
constexpr int RCEXL_RUN_TEST_US = 1500;
// These RCEXL pulse widths are arbitrary simulation markers, NOT verified
// ON/OFF commands, pinout or failsafe settings of any real RCEXL unit.
constexpr int STEER_CENTER_DEG = 90;
constexpr int GAS_CLOSED_DEG = 0;        // Simulation: calibrate real linkage.
constexpr int GAS_MAX_TEST_DEG = 135;
constexpr uint32_t LOW_BATTERY_MV = 10200; // Simulation threshold, not a BMS.
constexpr size_t SERIAL_LINE_SIZE = 48;
constexpr uint8_t SERIAL_BYTES_PER_LOOP = 32;

Servo servoLai;
Servo servoGa;
Servo escSignal;
Servo rcexlTestSignal;

enum VehicleState { STOPPED, ARMED, RUNNING };
VehicleState state = STOPPED;
const char* stopReason = "BOOT";
uint32_t lastValidCommandMs = 0;
uint32_t lastBatterySampleMs = 0;
uint32_t batteryMv = 0;
bool outputsReady = false;
char serialLine[SERIAL_LINE_SIZE];
size_t serialLength = 0;
bool discardLine = false;
bool discardPendingRx = false;

// Never wait for TX space: a flooded monitor must not block failsafe.
// Diagnostic replies may be dropped under load; control state remains valid.
void reply(const char* format, ...) {
  char message[128];
  va_list args;
  va_start(args, format);
  const int length = vsnprintf(message, sizeof(message), format, args);
  va_end(args);
  if (length > 0 && length < static_cast<int>(sizeof(message)) &&
      Serial.availableForWrite() >= length) {
    Serial.write(reinterpret_cast<const uint8_t*>(message), length);
  }
}

const char* stateName() {
  if (state == ARMED) return "ARMED";
  if (state == RUNNING) return "RUNNING";
  return "STOPPED";
}

void sampleBattery(bool force = false) {
  const uint32_t now = millis();
  if (force || static_cast<uint32_t>(now - lastBatterySampleMs) >= BATTERY_SAMPLE_MS) {
    lastBatterySampleMs = now;
    // WOKWI POT ONLY: raw 0..4095 => simulated pack 0..12.6 V.
    // VCC = 3V3, GND = GND. Never connect a real 3S pack to GPIO34.
    // Real hardware needs a divider, calibrated ADC and another conversion.
    batteryMv = static_cast<uint32_t>(analogRead(PIN_BATTERY)) * 12600UL / 4095UL;
  }
}

void updateLeds() {
  digitalWrite(LED_ARMED, state == ARMED ? HIGH : LOW);
  digitalWrite(LED_RUNNING, state == RUNNING ? HIGH : LOW);
  digitalWrite(LED_FAILSAFE, state == STOPPED ? HIGH : LOW);
}

void safeOutputs() {
  servoGa.write(GAS_CLOSED_DEG);
  escSignal.writeMicroseconds(ESC_STOP_US);
  rcexlTestSignal.writeMicroseconds(RCEXL_STOP_TEST_US);
  servoLai.write(STEER_CENTER_DEG);
  updateLeds();
}

void forceStop(const char* reason) {
  state = STOPPED;
  stopReason = reason;
  safeOutputs();                        // Apply outputs before diagnostics.
  discardLine = discardLine || serialLength != 0;
  serialLength = 0;
  discardPendingRx = Serial.available() > 0; // Drop commands queued before stop.
  reply("STOPPED: %s\n", reason);
}

// Level-sensitive: holding E-Stop prevents both ARM and RUN.
const char* activeInterlock() {
  if (digitalRead(PIN_ESTOP) == LOW) return "E_STOP";
  if (!outputsReady) return "PWM_ATTACH_FAILED";
  if (batteryMv < LOW_BATTERY_MV) return "LOW_BATTERY";
  return nullptr;
}

void enforceSafety() {
  sampleBattery();
  const char* fault = activeInterlock();
  if (fault != nullptr) {
    if (state != STOPPED || strcmp(stopReason, fault) != 0) forceStop(fault);
    return;
  }
  // Unsigned subtraction also handles millis() wraparound.
  if (state == RUNNING &&
      static_cast<uint32_t>(millis() - lastValidCommandMs) >= TIMEOUT_MS) {
    forceStop("TIMEOUT");
  }
}

void acceptedCommand() {
  if (state == RUNNING) lastValidCommandMs = millis();
}

bool parseNumber(const char* text, int minimum, int maximum, int& value) {
  if (text == nullptr || *text == '\0') return false;
  int result = 0;
  unsigned digits = 0;
  for (const char* p = text; *p; ++p) {
    if (*p < '0' || *p > '9' || ++digits > 3) return false;
    result = result * 10 + (*p - '0');
    if (result > maximum) return false;
  }
  if (result < minimum) return false;
  value = result;
  return true;
}

void processCommand(char* line) {
  while (*line == ' ' || *line == '\t') ++line;
  size_t length = strlen(line);
  while (length && (line[length - 1] == ' ' || line[length - 1] == '\t')) {
    line[--length] = '\0';
  }
  if (!length) return;                    // Empty lines are not heartbeat.
  for (char* p = line; *p; ++p) *p = static_cast<char>(toupper(static_cast<unsigned char>(*p)));
  char* argument = strpbrk(line, " \t");
  if (argument) {
    *argument++ = '\0';
    while (*argument == ' ' || *argument == '\t') ++argument;
  }

  // Check again at a complete frame; an overdue PING cannot rescue a timeout.
  const VehicleState stateBeforeCheck = state;
  sampleBattery(true);
  enforceSafety();
  if (stateBeforeCheck != STOPPED && state == STOPPED) return;
  if (strcmp(line, "STOP") == 0 && argument == nullptr) {
    forceStop("COMMAND");
    return;
  }
  if (strcmp(line, "BAT") == 0 && argument == nullptr) {
    acceptedCommand();
    reply("BAT=%.3f V SIM | %s | LAST_STOP=%s\n", batteryMv / 1000.0, stateName(), stopReason);
    return;
  }
  if (strcmp(line, "PING") == 0 && argument == nullptr) {
    acceptedCommand();
    reply("PING OK | %s\n", stateName());
    return;
  }
  if ((strcmp(line, "ARM") == 0 || strcmp(line, "RUN") == 0) && argument == nullptr) {
    const char* fault = activeInterlock();
    if (fault) {
      reply("REJECT: %s\n", fault);
      return;
    }
    if (strcmp(line, "ARM") == 0) {
      if (state != STOPPED) {
        reply("REJECT: ARM requires STOPPED\n");
        return;
      }
      state = ARMED;
      safeOutputs();
      reply("ARMED: outputs at safe idle\n");
    } else {
      if (state != ARMED) {
        reply("REJECT: RUN requires ARM\n");
        return;
      }
      state = RUNNING;
      safeOutputs();
      rcexlTestSignal.writeMicroseconds(RCEXL_RUN_TEST_US);
      acceptedCommand();
      reply("RUNNING: send PING or valid command before 5000 ms\n");
    }
    return;
  }

  const bool steer = strcmp(line, "STEER") == 0;
  const bool gas = strcmp(line, "GAS") == 0;
  const bool esc = strcmp(line, "ESC") == 0;
  if (steer || gas || esc) {
    int value = 0;
    if (!parseNumber(argument, steer ? 45 : 0, steer ? 135 : 100, value)) {
      reply("REJECT: %s requires integer %s\n", line, steer ? "45..135" : "0..100");
      return;
    }
    if (state != RUNNING) {
      reply("REJECT: controls require ARM then RUN\n");
      return;
    }
    if (steer) servoLai.write(value);
    if (gas) servoGa.write(map(value, 0, 100, GAS_CLOSED_DEG, GAS_MAX_TEST_DEG));
    if (esc) escSignal.writeMicroseconds(map(value, 0, 100, ESC_STOP_US, ESC_MAX_TEST_US));
    acceptedCommand();
    reply("OK %s %d\n", line, value);
    return;
  }
  reply("REJECT: invalid command or extra argument\n");
}

void serviceSerial() {
  for (uint8_t count = 0; count < SERIAL_BYTES_PER_LOOP && Serial.available(); ++count) {
    enforceSafety();
    const char c = static_cast<char>(Serial.read());
    const bool endOfLine = c == '\r' || c == '\n';
    if (discardPendingRx) {
      discardLine = !endOfLine;
      if (!Serial.available()) discardPendingRx = false;
      continue;
    }
    if (endOfLine) {
      if (discardLine) {
        discardLine = false;
        serialLength = 0;
        reply("REJECT: discarded incomplete, oversized or invalid line\n");
      } else if (serialLength) {
        serialLine[serialLength] = '\0';
        serialLength = 0;
        processCommand(serialLine);
      }
      continue;
    }
    if (discardLine) continue;
    if ((static_cast<uint8_t>(c) < 32 && c != '\t') || static_cast<uint8_t>(c) > 126 ||
        serialLength >= sizeof(serialLine) - 1) {
      discardLine = true;               // Never execute a valid-looking suffix.
      serialLength = 0;
      continue;
    }
    serialLine[serialLength++] = c;
  }
}

void setup() {
  Serial.setTxBufferSize(256);
  Serial.begin(115200);
  pinMode(PIN_ESTOP, INPUT_PULLUP);
  pinMode(PIN_BATTERY, INPUT);
  pinMode(LED_ARMED, OUTPUT);
  pinMode(LED_RUNNING, OUTPUT);
  pinMode(LED_FAILSAFE, OUTPUT);
  updateLeds();
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_BATTERY, ADC_11db);

  // Set each safe target immediately after attach. A real output gate is still
  // needed to exclude boot/reset transients before firmware starts.
  servoLai.setPeriodHertz(50);
  servoLai.attach(PIN_STEER, 500, 2500);
  servoLai.write(STEER_CENTER_DEG);
  servoGa.setPeriodHertz(50);
  servoGa.attach(PIN_GAS, 500, 2500);
  servoGa.write(GAS_CLOSED_DEG);
  escSignal.setPeriodHertz(50);
  escSignal.attach(PIN_ESC, 1000, 2000);
  escSignal.writeMicroseconds(ESC_STOP_US);
  rcexlTestSignal.setPeriodHertz(50);
  rcexlTestSignal.attach(PIN_RCEXL_TEST, 1000, 2000);
  rcexlTestSignal.writeMicroseconds(RCEXL_STOP_TEST_US);
  outputsReady = servoLai.attached() && servoGa.attached() &&
                 escSignal.attached() && rcexlTestSignal.attached();
  safeOutputs();
  sampleBattery(true);
  reply("READY: STOPPED | WOKWI battery + RCEXL PWM TEST ONLY\n");
  enforceSafety();
}

void loop() {
  enforceSafety();
  serviceSerial();
  enforceSafety();
  delay(1);                            // Yield to ESP32 tasks; no Serial wait.
}
