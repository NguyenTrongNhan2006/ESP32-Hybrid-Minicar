#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p .build/sketch .build/esp32
cp firmware/wokwi/sketch.ino .build/sketch/sketch.ino
arduino-cli compile --fqbn esp32:esp32:esp32 --warnings all \
  --output-dir "$PWD/.build/esp32" .build/sketch
