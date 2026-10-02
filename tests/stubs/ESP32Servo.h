#pragma once
#include "Arduino.h"
// Deliberately models requested outputs only, not the ESP32 PWM peripheral.
struct Servo {
  int pin = -1, angle = -1, pulse = -1, hz = 0;
  void setPeriodHertz(int value) { hz = value; }
  int attach(int value, int, int) { pin = value == failedAttachPin ? -1 : value; return pin; }
  bool attached() { return pin >= 0; }
  void write(int value) { angle = value; }
  void writeMicroseconds(int value) { pulse = value; }
};
