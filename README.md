# ESP32 Hybrid Minicar

Firmware **C thuần trên ESP-IDF** cho ESP32-WROOM/DevKit, điều khiển thử bàn bằng điện thoại hoặc USB Serial. FSM STOPPED → ARMED → RUNNING; giữ nguyên toàn bộ GPIO đã thống nhất.

**Đã kiểm tra logic và biên dịch; chưa xác nhận xe thật hoặc khả năng chịu nhiễu khi thi.** BAT hiện là biến trở mô phỏng; GPIO26 chỉ là PWM test, chưa điều khiển ngắt đánh lửa thật.

## Bắt đầu

1. [Mở VS Code, build và nạp ESP-IDF](docs/ESP32_WROOM.md).
2. [Đấu chân, nguồn và hai servo](docs/PINOUT.md).
3. [Điện thoại](docs/PHONE.md): nối Wi-Fi `ESP32-Hybrid`, mật khẩu ở USB Serial, mở `http://192.168.4.1`.
4. [FSM, flowchart và sơ đồ hệ thống](schematic/README.md).
5. [Test nhiễu và failsafe trước khi chạy xe](docs/HARDWARE_TESTS.md).

| Thư mục | Nội dung |
|---|---|
| `firmware/esp-idf/` | Firmware chính bằng C, ESP-IDF 6.0.2; timeout **500 ms**, PING mỗi 100 ms khi giữ RUN |
| `firmware/wokwi/` | Arduino/Wokwi, timeout **5 giây**, Serial giả lập điện thoại |
| `docs/` | [About/phạm vi](docs/ABOUT.md), hướng dẫn, pinout, review và test |
| `schematic/` | Sơ đồ SVG tự tạo, mở trực tiếp và phóng to |
| `tests/` | Test C, Arduino, logic giao diện và đối chiếu GPIO |
| `scripts/` | Build hỗ trợ |

Mở `ESP32-Hybrid-Minicar-IDF.code-workspace` để VS Code chọn đúng project C. Trên Windows nên clone vào đường dẫn ngắn không dấu, ví dụ `N:\ESP32-Hybrid-Minicar`; toolchain trên máy hiện tại đã lỗi khi build trong đường dẫn OneDrive có dấu.

## Lệnh chung

Serial **115200**, kết thúc newline. `ARM`, `RUN`, `PING`, `STOP`, `STEER 45..135`, `GAS 0..100`, `ESC 0..100`, `BAT`. RUN chỉ sau ARM; điều khiển chỉ khi RUNNING. Lệnh sai không gia hạn timeout. Sau STOP/failsafe phải ARM rồi RUN lại. IDF khóa quyền điều khiển theo nguồn đã ARM; nguồn khác vẫn được STOP.

Đích dừng: ga 0°, lái 90°, ESC 1000 µs, GPIO26 test 1000 µs, LED đỏ. Các góc/xung là cấu hình thử, cần hiệu chuẩn cơ khí và kiểm tra manual ESC/servo trước khi ghép tải.

## Kiểm tra tự động

GitHub Actions build Arduino và ESP-IDF, chạy test logic, xuất firmware cùng ZIP Wokwi. Test host dùng phần cứng giả lập, không đo nhiễu hay dạng sóng thực.

```sh
mkdir -p .build
gcc -std=c11 -Wall -Wextra -Werror -pedantic -Ifirmware/esp-idf/main tests/controller_test.c -o .build/controller_test
.build/controller_test
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Itests/stubs tests/firmware_test.cpp -o .build/firmware_test
.build/firmware_test
node tests/check_wiring.mjs
node tests/phone_ui_test.mjs
```

Windows: tạo `.build` bằng PowerShell, thêm `.exe` cho hai executable test. [Test Wokwi](docs/WOKWI_TESTS.md). Ảnh/tài liệu gốc giữ riêng tại máy trong `references/`, không nằm trong repo công khai.
