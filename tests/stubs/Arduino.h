#pragma once
#include <cstdint>
#include <cstddef>
#include <deque>
#include <string>
#include <stdexcept>

constexpr int LOW = 0, HIGH = 1, INPUT = 0, OUTPUT = 1, INPUT_PULLUP = 2;
constexpr int ADC_11db = 3;
inline uint32_t testNow = 0;
inline int testEstop = HIGH, testAdc = 4095, failedAttachPin = -1;
inline int pinValues[40] = {}, pinModes[40] = {};
inline uint32_t millis() { return testNow; }
inline void delay(uint32_t ms) { testNow += ms; }
inline void pinMode(int pin, int mode) { pinModes[pin] = mode; }
inline void digitalWrite(int pin, int value) { pinValues[pin] = value; }
inline int digitalRead(int pin) { return pin == 27 ? testEstop : pinValues[pin]; }
inline int analogRead(int) { return testAdc; }
inline void analogReadResolution(int) {}
inline void analogSetPinAttenuation(int, int) {}
inline long map(long x, long a, long b, long c, long d) { return (x-a)*(d-c)/(b-a)+c; }

struct TestSerial {
  std::deque<char> input;
  std::string output;
  int txSpace = 256;
  void begin(int) {}
  void setTxBufferSize(size_t) {}
  int available() { return static_cast<int>(input.size()); }
  int availableForWrite() { return txSpace; }
  int read() { const auto c = input.front(); input.pop_front(); return static_cast<unsigned char>(c); }
  size_t write(const uint8_t* p, size_t n) {
    if (static_cast<int>(n) > txSpace) throw std::runtime_error("Blocking TX attempted");
    output.append(reinterpret_cast<const char*>(p), n);
    return n;
  }
  void feed(const std::string& text) { for (char c : text) input.push_back(c); }
};
inline TestSerial Serial;
