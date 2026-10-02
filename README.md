# ESP32 Hybrid Minicar

Firmware mô phỏng xe hybrid mini trên **Wokwi ESP32 DevKit**, hướng tới thử bàn trên board **ESP32-WROOM**. Điều khiển bằng Serial giả lập điện thoại; chưa triển khai BLE/Wi-Fi hay ứng dụng điện thoại.

FSM: `STOPPED → ARM → ARMED → RUN → RUNNING`. E-Stop, pin yếu, timeout hoặc `STOP` đưa về `STOPPED`; phải ARM/RUN lại. GPIO26 chỉ phát **PWM thử nghiệm**, chưa có thông số ON/OFF thật của RCEXL.

## Bắt đầu với Wokwi

1. Mở [project Wokwi gốc](https://wokwi.com/projects/476564986011683841) hoặc tạo project ESP32 mới.
2. Thay nội dung `sketch.ino`, `diagram.json`, `libraries.txt` bằng ba file ở thư mục gốc repo này. Link gốc không tự cập nhật theo GitHub.
3. Start Simulation; Serial Monitor **115200 baud**, mỗi lệnh kết thúc bằng Enter/newline.
4. Biến trở mặc định ở mức cao. Gửi từng dòng, cách nhau dưới 5 giây:

```text
BAT
ARM
RUN
STEER 110
GAS 20
ESC 20
PING
STOP
```

Xem [toàn bộ test Wokwi](docs/WOKWI_TESTS.md), [review trước khi sửa](docs/REVIEW.md), và [hướng dẫn ESP32-WROOM](docs/ESP32_WROOM.md).

## GPIO và sơ đồ đang dùng

| Chức năng | GPIO | Kết nối / giá trị mô phỏng |
|---|---:|---|
| Servo lái | 18 | servo1 PWM; 45–135°, giữa 90° |
| Servo ga | 19 | servo2 PWM; GAS 0–100% → 0–135° |
| ESC BLDC | 23 | Logic Analyzer D0; ESC 0–100% → 1000–1600 µs |
| RCEXL PWM test | 26 | Logic Analyzer D1; STOPPED/ARMED 1000 µs, RUNNING 1500 µs |
| E-Stop | 27 | INPUT_PULLUP, nút nhấn xuống GND |
| Biến trở pin | 34 | SIG; VCC **3V3**; GND chung |
| LED ARMED xanh | 25 | r2 330 Ω → led3 → GND |
| LED STOPPED/FAILSAFE đỏ | 32 | r1 330 Ω → led1 → GND |
| LED RUNNING vàng | 33 | r3 330 Ω → led2 → GND |

Tất cả PWM dùng 50 Hz (chu kỳ 20 ms). Các nhãn D0/D1 ở đây là **kênh Logic Analyzer**, không phải chân D0/D1 của ESP32. Không thay GPIO so với yêu cầu. Sơ đồ gốc đã được sửa nguồn biến trở 5V → 3V3 và hai dây LED xanh/vàng; đặt biến trở khởi động ở 1023/1023.

## Giao thức Serial

| Lệnh | Điều kiện / tác dụng |
|---|---|
| `ARM` | Chỉ từ STOPPED, không E-Stop, pin ≥ 10.2 V giả lập, PWM attach thành công; đầu ra vẫn idle |
| `RUN` | Chỉ từ ARMED với các điều kiện trên; ga/ESC vẫn bằng 0 cho đến lệnh điều khiển |
| `PING` | Phản hồi trạng thái; làm mới timeout nếu RUNNING |
| `BAT` | In điện áp **giả lập**, trạng thái và lý do dừng gần nhất; làm mới timeout nếu RUNNING |
| `STEER 45..135` | Chỉ RUNNING; góc nguyên, làm mới timeout khi được chấp nhận |
| `GAS 0..100` | Chỉ RUNNING; phần trăm nguyên, làm mới timeout khi được chấp nhận |
| `ESC 0..100` | Chỉ RUNNING; phần trăm nguyên, làm mới timeout khi được chấp nhận |
| `STOP` | Dừng từ mọi trạng thái; xóa lệnh đang chờ trong RX |

Không phân biệt chữ hoa/thường; nhận LF, CR hoặc CRLF, khoảng trắng/tab quanh lệnh. Số chỉ gồm 1–3 chữ số thập phân; không nhận dấu, số lẻ, hậu tố hoặc đối số thừa. `ARM`/`RUN` lặp sai trạng thái bị từ chối. Lệnh sai, dòng trống và dòng chưa hoàn chỉnh không làm mới heartbeat.

Khi RUNNING, **≥ 5000 ms** từ lệnh được chấp nhận gần nhất → STOPPED. Nên gửi PING mỗi 1 giây để có khoảng dự phòng. Bộ đọc không chờ newline, xử lý tối đa 32 byte mỗi vòng; dòng quá 47 ký tự/binary bị bỏ đến newline. Khi dừng, bỏ dữ liệu RX đang chờ; nếu đang gửi dở dòng, gửi một newline rồi ARM/RUN mới. Log có thể bị bỏ khi bộ đệm TX đầy để vòng điều khiển tiếp tục.

Mọi lần dừng: ga 0°, ESC 1000 µs, lái 90°, GPIO26 test 1000 µs, LED đỏ sáng. Không tự khôi phục khi thả E-Stop, tăng điện áp hay nhận PING.

## Kiểm thử và build

Host tests biên dịch trực tiếp `sketch.ino` với giả lập GPIO/ADC/Serial/Servo; kiểm tra logic, **không xác nhận dạng sóng PWM thật**:

```sh
mkdir -p .build
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Itests/stubs tests/firmware_test.cpp -o .build/firmware_test
.build/firmware_test
node tests/check_wiring.mjs
```

PowerShell: tạo `.build` bằng `New-Item -ItemType Directory -Force .build`; đặt tên executable `.build/firmware_test.exe` và chạy nó.

GitHub Actions chạy các test trên và biên dịch target `esp32:esp32:esp32` với Arduino CLI **1.5.1**, Arduino-ESP32 **3.3.12**, ESP32Servo **3.2.1**. Mỗi lần build thành công có artifact firmware và ZIP Wokwi. `libraries.txt` khóa phiên bản ESP32Servo; phiên bản core của Wokwi web do dịch vụ quản lý.

Build bằng CLI trên máy đã cài core/thư viện:

```sh
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.12 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib install ESP32Servo@3.2.1
bash scripts/build.sh
```

Windows chạy `./scripts/build.ps1`. Script tạo bản staging có tên folder/sketch hợp lệ cho Arduino; nguồn chính vẫn là `sketch.ino` ở root.

## Phạm vi phần cứng

Bản này là **mô phỏng và thử tín hiệu trên bàn**. `raw × 12600 / 4095` là phép quy đổi biến trở Wokwi, không phải phép đo pin 3S ngoài đời. Ngưỡng 10.2 V, ga 0–135°, ESC 1000–1600 µs và GPIO26 1000/1500 µs cần được đánh giá riêng cho phần cứng đã chọn. Firmware không thay mạch ngắt động cơ độc lập khi MCU treo/mất điện.

Các file sơ đồ/PDF tham khảo và bản Wokwi ban đầu được giữ riêng trong thư mục `references/` ở máy làm việc, không nằm trong repo công khai. Các sơ đồ tham khảo có GPIO tùy chọn/kiến trúc khác; **bảng chân và `diagram.json` ở root là cấu hình của firmware này**. Không đấu receiver/relay theo sơ đồ tùy chọn vào các chân đang dùng cho LED/E-Stop.

Nguồn đối chiếu: [Espressif DevKitC V4](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/esp32-devkitc/user_guide.html), [Arduino-ESP32 ADC](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/adc.html), [ESP32Servo](https://github.com/madhephaestus/ESP32Servo), [Wokwi potentiometer](https://docs.wokwi.com/parts/wokwi-potentiometer).
