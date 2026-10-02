$ErrorActionPreference = 'Stop'
Push-Location (Split-Path $PSScriptRoot -Parent)
try {
    New-Item -ItemType Directory -Force -Path '.build/sketch', '.build/esp32' | Out-Null
    Copy-Item -LiteralPath 'firmware/wokwi/sketch.ino' -Destination '.build/sketch/sketch.ino'
    arduino-cli compile --fqbn esp32:esp32:esp32 --warnings all --output-dir '.build/esp32' '.build/sketch'
    if ($LASTEXITCODE -ne 0) { throw 'ESP32 build failed' }
} finally { Pop-Location }
