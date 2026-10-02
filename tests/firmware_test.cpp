#include <cstdlib>
#include <iostream>
#include <limits>
#include "../firmware/wokwi/sketch.ino"

int assertions = 0;
#define CHECK(condition) do { ++assertions; if (!(condition)) { \
  std::cerr << "FAIL line " << __LINE__ << ": " #condition "\n"; std::exit(1); } } while (0)

void reset(int adc = 4095, int estop = HIGH, int failPin = -1) {
  testNow = 0; testAdc = adc; testEstop = estop; failedAttachPin = failPin;
  Serial = TestSerial{};
  servoLai = Servo{}; servoGa = Servo{}; escSignal = Servo{}; rcexlTestSignal = Servo{};
  state = STOPPED; stopReason = "BOOT"; outputsReady = false;
  lastValidCommandMs = lastBatterySampleMs = batteryMv = 0;
  serialLength = 0; discardLine = discardPendingRx = false;
  setup();
}

void send(const std::string& text) {
  Serial.feed(text);
  unsigned iterations = 0;
  while (Serial.available()) { loop(); CHECK(++iterations < 10000); }
}

void idleOutputs() {
  CHECK(servoGa.angle == 0);
  CHECK(servoLai.angle == 90);
  CHECK(escSignal.pulse == 1000);
}

void stopped() {
  CHECK(state == STOPPED);
  idleOutputs();
  CHECK(rcexlTestSignal.pulse == 1000);
  CHECK(pinValues[32] == HIGH && pinValues[25] == LOW && pinValues[33] == LOW);
}

void run() {
  send("ARM\n"); CHECK(state == ARMED);
  send("RUN\n"); CHECK(state == RUNNING);
}

int main() {
  reset(); stopped();
  CHECK(pinModes[27] == INPUT_PULLUP);
  CHECK(servoLai.pin == 18 && servoGa.pin == 19 && escSignal.pin == 23 && rcexlTestSignal.pin == 26);
  CHECK(servoLai.hz == 50 && servoGa.hz == 50 && escSignal.hz == 50 && rcexlTestSignal.hz == 50);
  send("RUN\nGAS 100\nESC 100\nSTEER 45\n"); stopped();
  send("ARM\n"); CHECK(state == ARMED); idleOutputs();
  CHECK(pinValues[25] == HIGH && pinValues[33] == LOW && pinValues[32] == LOW);
  send("GAS 100\nESC 100\nSTEER 135\n"); idleOutputs();
  send("RUN\n"); CHECK(state == RUNNING); idleOutputs();
  CHECK(rcexlTestSignal.pulse == 1500 && pinValues[33] == HIGH && pinValues[25] == LOW);
  send("STEER 45\nGAS 100\nESC 100\n");
  CHECK(servoLai.angle == 45 && servoGa.angle == 135 && escSignal.pulse == 1600);
  send("STEER 135\nGAS 0\nESC 0\n");
  CHECK(servoLai.angle == 135 && servoGa.angle == 0 && escSignal.pulse == 1000);
  send("STOP\n"); stopped(); send("RUN\n"); stopped();
  std::cout << "PASS boot, FSM, GPIO, output ranges, STOP and re-arm\n";

  reset(); run(); send("GAS 50\nESC 50\nSTEER 110\n");
  const auto timestamp = lastValidCommandMs;
  const char* invalid[] = {"GAS abc", "ESC abc", "GAS 50xyz", "ESC 10 20", "STEER 44",
    "STEER 136", "GAS -1", "ESC 101", "GAS 1.5", "GAS +1", "GAS", "ESC 9999999999999",
    "ARM", "RUN", "PING junk", "BAT junk", "STOP junk", "wat", "", "   "};
  for (const char* text : invalid) {
    send(std::string(text) + "\n");
    CHECK(state == RUNNING && lastValidCommandMs == timestamp);
    CHECK(servoGa.angle == 67 && escSignal.pulse == 1300 && servoLai.angle == 110);
  }
  testNow = timestamp + 5000; loop(); stopped();
  std::cout << "PASS malformed/rejected commands cannot refresh heartbeat\n";

  for (const char* valid : {"PING\n", "BAT\n", "STEER 90\n", "GAS 0\n", "ESC 0\n"}) {
    reset(); run(); testNow = lastValidCommandMs + 4999;
    send(valid); CHECK(state == RUNNING);
    const auto accepted = lastValidCommandMs;
    testNow = accepted + 4999; loop(); CHECK(state == RUNNING);
    testNow = accepted + 5000; loop(); stopped();
  }
  reset(); run(); testNow = lastValidCommandMs + 5000;
  send("PING\nARM\nRUN\nESC 100\n"); stopped();
  CHECK(std::string(stopReason) == "TIMEOUT"); run();
  std::cout << "PASS 4999/5000 ms deadline, BAT keepalive, late queue discarded\n";

  reset(); run(); send("GAS 100\nESC 100\nSTEER 45\n");
  testEstop = LOW; loop(); stopped();
  send("ARM\nRUN\nGAS 100\n"); stopped();
  testEstop = HIGH; loop(); send("RUN\n"); stopped(); run();
  reset(4095, LOW); stopped(); send("ARM\nRUN\n"); stopped();
  std::cout << "PASS E-Stop held, recovery and E-Stop at boot\n";

  reset(); run(); send("GAS 100\nESC 100\nSTEER 135\n");
  testAdc = 3314; testNow += 20; loop(); stopped();
  CHECK(std::string(stopReason) == "LOW_BATTERY");
  send("ARM\nRUN\n"); stopped();
  testAdc = 4095; testNow += 20; loop(); send("RUN\n"); stopped(); run();
  reset(3315); run(); CHECK(batteryMv == 10200);
  reset(3314); stopped(); send("ARM\nRUN\n"); stopped();
  reset(); send("ARM\n"); testAdc = 0; send("RUN\n"); stopped();
  reset(); send("ARM\n"); testAdc = 0; testNow += 20; loop(); stopped();
  std::cout << "PASS battery threshold, boot/ARM/RUN interlocks and recovery\n";

  reset(); run(); send("ESC 100"); CHECK(escSignal.pulse == 1000);
  testEstop = LOW; loop(); stopped();
  testEstop = HIGH; send("\nARM\nRUN\n"); CHECK(state == RUNNING);
  send("ESC 100"); testNow = lastValidCommandMs + 5000; loop(); stopped();
  send("\n"); run(); send(std::string(200, 'X') + "ESC 100\n");
  CHECK(escSignal.pulse == 1000);
  send(std::string("ESC 100\0", 8) + "\n"); CHECK(escSignal.pulse == 1000);
  send(" eSc\t50 \r\n"); CHECK(escSignal.pulse == 1300);
  send("steer 135\rgas 100\n"); CHECK(servoLai.angle == 135 && servoGa.angle == 135);
  std::cout << "PASS partial lines, overflow, NUL, whitespace, case and CR/LF\n";

  reset(); run(); send("ESC 100\n");
  Serial.txSpace = 0; Serial.feed(std::string(10000, 'X'));
  const auto queued = Serial.available(); loop();
  CHECK(queued - Serial.available() <= 32);
  testEstop = LOW; loop(); stopped();
  CHECK(Serial.available() > 9000);
  reset(); run(); Serial.txSpace = 0;
  for (int i = 0; i < 5100; ++i) { Serial.feed("garbage\n"); loop(); }
  stopped(); CHECK(std::string(stopReason) == "TIMEOUT");
  std::cout << "PASS RX flood and TX backpressure cannot delay safety loop\n";

  reset(); testNow = std::numeric_limits<uint32_t>::max() - 2500; run();
  const auto beforeWrap = lastValidCommandMs;
  testNow = beforeWrap + 4999; loop(); CHECK(state == RUNNING);
  testNow = beforeWrap + 5000; loop(); stopped();
  for (int pin : {18, 19, 23, 26}) {
    reset(4095, HIGH, pin); stopped(); send("ARM\nRUN\n"); stopped();
    CHECK(std::string(stopReason) == "PWM_ATTACH_FAILED");
  }
  reset(); run(); send("STOP\nARM\nRUN\nESC 100\n"); stopped(); run();
  std::cout << "PASS millis wrap, PWM attach failure, queued commands after STOP\n";
  std::cout << "All firmware tests passed (" << assertions << " assertions).\n";
}
