#include "controller.h"
#include "board.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
VehicleState state = STOPPED;
const char* stopReason = "BOOT";
uint32_t lastValidCommandMs = 0;
uint32_t lastBatterySampleMs = 0;
uint32_t batteryMv = 0;
bool outputsReady = false;
bool adcOk = false;
char serialLine[SERIAL_LINE_SIZE];
size_t serialLength = 0;
bool discardLine = false;
bool discardPendingRx = false;
static uint32_t owner, currentSource, currentSequence, lastSequence, stopCount;
static bool commandAccepted;
static int steerTarget = 90, gasTarget, escTarget;
static char lastReply[128];

// Never wait for TX space: a flooded monitor must not block failsafe.
// Diagnostic replies may be dropped under load; control state remains valid.
void reply(const char* format, ...) {
  char message[128];
  va_list args;
  va_start(args, format);
  const int length = vsnprintf(message, sizeof(message), format, args);
  va_end(args);
  if (length > 0 && length < (int)sizeof(message)) {
    memcpy(lastReply, message, (size_t)length + 1);
    board_tx_try(message, (size_t)length);
  }
}

const char* stateName(void) {
  if (state == ARMED) return "ARMED";
  if (state == RUNNING) return "RUNNING";
  return "STOPPED";
}

void sampleBattery(bool force) {
  const uint32_t now = board_millis();
  if (force || (uint32_t)(now - lastBatterySampleMs) >= BATTERY_SAMPLE_MS) {
    lastBatterySampleMs = now;
    // WOKWI POT ONLY: raw 0..4095 => simulated pack 0..12.6 V.
    // VCC = 3V3, GND = GND. Never connect a real 3S pack to GPIO34.
    // Real hardware needs a divider, calibrated ADC and another conversion.
    int raw = board_adc_raw();
    adcOk = raw >= 0 && raw <= 4095;
    batteryMv = adcOk ? (uint32_t)raw * 12600U / 4095U : 0;
  }
}

void updateLeds(void) {
  board_gpio_write(LED_ARMED, state == ARMED ? HIGH : LOW);
  board_gpio_write(LED_RUNNING, state == RUNNING ? HIGH : LOW);
  board_gpio_write(LED_FAILSAFE, state == STOPPED ? HIGH : LOW);
}

void safeOutputs(void) {
  steerTarget = 90; gasTarget = escTarget = 0;
  board_angle(PIN_GAS, GAS_CLOSED_DEG);
  board_pulse(PIN_ESC, ESC_STOP_US);
  board_pulse(PIN_RCEXL_TEST, RCEXL_STOP_TEST_US);
  board_angle(PIN_STEER, STEER_CENTER_DEG);
  updateLeds();
}

void forceStop(const char* reason) {
  ++stopCount;
  owner = lastSequence = 0;
  state = STOPPED;
  stopReason = reason;
  safeOutputs();                        // Apply outputs before diagnostics.
  discardLine = discardLine || serialLength != 0;
  serialLength = 0;
  discardPendingRx = board_rx_available() > 0; // Drop commands queued before stop.
  reply("STOPPED: %s\n", reason);
}

// Level-sensitive: holding E-Stop prevents both ARM and RUN.
const char* activeInterlock(void) {
  if (board_estop_pressed()) return "E_STOP";
  if (!outputsReady) return "HARDWARE_FAULT";
  if (!adcOk) return "ADC_READ_FAILED";
  if (batteryMv < LOW_BATTERY_MV) return "LOW_BATTERY";
  return NULL;
}

const char* enforceSafety(void) {
  outputsReady = board_is_ready();
  sampleBattery(false);
  const char* fault = activeInterlock();
  if (fault != NULL) {
    if (state != STOPPED || strcmp(stopReason, fault) != 0) forceStop(fault);
    return fault;
  }
  // Unsigned subtraction also handles board_millis() wraparound.
  if (state == RUNNING &&
      (uint32_t)(board_millis() - lastValidCommandMs) >= TIMEOUT_MS) {
    forceStop("TIMEOUT");
    return "TIMEOUT";
  }
  return NULL;
}

void acceptedCommand(void) {
  commandAccepted = true;
  if (currentSource && owner == currentSource) lastSequence = currentSequence;
  if (state == RUNNING) lastValidCommandMs = board_millis();
}

bool parseNumber(const char* text, int minimum, int maximum, int* value) {
  if (text == NULL || *text == '\0') return false;
  int result = 0;
  unsigned digits = 0;
  for (const char* p = text; *p; ++p) {
    if (*p < '0' || *p > '9' || ++digits > 3) return false;
    result = result * 10 + (*p - '0');
    if (result > maximum) return false;
  }
  if (result < minimum) return false;
  *value = result;
  return true;
}

void processCommand(char* line) {
  commandAccepted = false;
  while (*line == ' ' || *line == '\t') ++line;
  size_t length = strlen(line);
  while (length && (line[length - 1] == ' ' || line[length - 1] == '\t')) {
    line[--length] = '\0';
  }
  if (!length) return;                    // Empty lines are not heartbeat.
  for (char* p = line; *p; ++p) *p = (char)toupper((unsigned char)*p);
  char* argument = strpbrk(line, " \t");
  if (argument) {
    *argument++ = '\0';
    while (*argument == ' ' || *argument == '\t') ++argument;
  }

  // Check again at a complete frame; an overdue PING cannot rescue a timeout.
  const VehicleState stateBeforeCheck = state;
  sampleBattery(true);
  const char* frameFault = enforceSafety();
  if (stateBeforeCheck != STOPPED && state == STOPPED) return;
  if (strcmp(line, "STOP") == 0 && argument == NULL) {
    forceStop("COMMAND");
    commandAccepted = true;
    return;
  }
  if (state != STOPPED && owner != currentSource) {
    reply("REJECT: another source owns ARM; STOP first\n");
    return;
  }
  if (currentSource && (currentSequence == 0 ||
      (state != STOPPED && currentSequence <= lastSequence))) {
    reply("REJECT: stale phone sequence\n");
    return;
  }
  if (strcmp(line, "BAT") == 0 && argument == NULL) {
    acceptedCommand();
    reply("BAT=%.3f V SIM | %s | LAST_STOP=%s\n", batteryMv / 1000.0, stateName(), stopReason);
    return;
  }
  if (strcmp(line, "PING") == 0 && argument == NULL) {
    acceptedCommand();
    reply("PING OK | %s\n", stateName());
    return;
  }
  if ((strcmp(line, "ARM") == 0 || strcmp(line, "RUN") == 0) && argument == NULL) {
    // Retain an E-Stop edge consumed during this frame's safety check.
    const char* fault = frameFault ? frameFault : activeInterlock();
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
      owner = currentSource;
      lastSequence = 0;
      safeOutputs();
      acceptedCommand();
      reply("ARMED: outputs at safe idle\n");
    } else {
      if (state != ARMED) {
        reply("REJECT: RUN requires ARM\n");
        return;
      }
      state = RUNNING;
      safeOutputs();
      board_pulse(PIN_RCEXL_TEST, RCEXL_RUN_TEST_US);
      acceptedCommand();
      reply("RUNNING: send PING or valid command before %u ms\n", TIMEOUT_MS);
    }
    return;
  }

  const bool steer = strcmp(line, "STEER") == 0;
  const bool gas = strcmp(line, "GAS") == 0;
  const bool esc = strcmp(line, "ESC") == 0;
  if (steer || gas || esc) {
    int value = 0;
    if (!parseNumber(argument, steer ? 45 : 0, steer ? 135 : 100, &value)) {
      reply("REJECT: %s requires integer %s\n", line, steer ? "45..135" : "0..100");
      return;
    }
    if (state != RUNNING) {
      reply("REJECT: controls require ARM then RUN\n");
      return;
    }
    if (steer) { steerTarget = value; board_angle(PIN_STEER, value); }
    if (gas) { gasTarget = value; board_angle(PIN_GAS, value * GAS_MAX_TEST_DEG / 100); }
    if (esc) { escTarget = value; board_pulse(PIN_ESC, ESC_STOP_US + value * (ESC_MAX_TEST_US - ESC_STOP_US) / 100); }
    acceptedCommand();
    reply("OK %s %d\n", line, value);
    return;
  }
  reply("REJECT: invalid command or extra argument\n");
}

void serviceSerial(void) {
  for (uint8_t count = 0; count < SERIAL_BYTES_PER_LOOP && board_rx_available(); ++count) {
    enforceSafety();
    const char c = (char)board_rx_read();
    const bool endOfLine = c == '\r' || c == '\n';
    if (discardPendingRx) {
      discardLine = !endOfLine;
      if (!board_rx_available()) discardPendingRx = false;
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
        currentSource = currentSequence = 0;
        processCommand(serialLine);
      }
      continue;
    }
    if (discardLine) continue;
    if (((uint8_t)c < 32 && c != '\t') || (uint8_t)c > 126 ||
        serialLength >= sizeof(serialLine) - 1) {
      discardLine = true;               // Never execute a valid-looking suffix.
      serialLength = 0;
      continue;
    }
    serialLine[serialLength++] = c;
  }
}


void vehicle_init(void) {
  state = STOPPED;
  stopReason = "BOOT";
  lastValidCommandMs = lastBatterySampleMs = batteryMv = 0;
  serialLength = 0;
  discardLine = discardPendingRx = false;
  owner = currentSource = currentSequence = lastSequence = stopCount = 0;
  commandAccepted = false;
  outputsReady = board_is_ready();
  adcOk = false;
  safeOutputs();
  sampleBattery(true);
  reply("READY: ESP-IDF C | STOPPED | BAT + RCEXL PWM TEST ONLY\n");
  enforceSafety();
}

void vehicle_poll(void) {
  enforceSafety();
  serviceSerial();
  enforceSafety();
}

bool vehicle_execute(const char* line, uint32_t source, uint32_t sequence) {
  char copy[SERIAL_LINE_SIZE];
  size_t length = strlen(line);
  if (!length || length >= sizeof(copy)) return false;
  for (size_t i = 0; i < length; ++i) {
    if (((uint8_t)line[i] < 32 && line[i] != '\t') || (uint8_t)line[i] > 126) return false;
  }
  memcpy(copy, line, length + 1);
  currentSource = source;
  currentSequence = sequence;
  processCommand(copy);
  enforceSafety();
  return commandAccepted;
}

void vehicle_stop(const char* reason) { forceStop(reason); }

vehicle_status_t vehicle_status(void) {
  vehicle_status_t status = {
    .state = state, .battery_mv = batteryMv, .owner = owner, .stop_count = stopCount,
    .steer_deg = steerTarget, .gas_percent = gasTarget, .esc_percent = escTarget,
    .last_stop = stopReason
  };
  memcpy(status.reply, lastReply, sizeof(status.reply));
  return status;
}
