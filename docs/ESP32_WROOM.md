# VS Code + Espressif IDF + C

Project chính ở `firmware/esp-idf/`; điểm vào là `main/main.c:app_main`. Dùng **ESP-IDF 6.0.2**, target **esp32**, flash 4 MB theo cấu hình DevKit dùng trong project. Kiểm tra dung lượng flash/model thật trước khi nạp. Không chọn S3/C3 cho ESP32-WROOM cổ điển.

## Mở và build

1. Clone/download repo vào đường dẫn không dấu, ví dụ `N:\ESP32-Hybrid-Minicar`. Toolchain Windows trên máy làm việc đã lỗi khi dùng đường dẫn OneDrive có dấu.
2. Mở `ESP32-Hybrid-Minicar-IDF.code-workspace` bằng VS Code; workspace chọn sẵn `firmware/esp-idf`.
3. Trong extension Espressif IDF, chọn môi trường **6.0.2** đã cài. Mở **ESP-IDF Terminal** để dùng đúng Python, CMake, Ninja và compiler.
4. Chạy `idf.py set-target esp32` một lần khi mới tạo cấu hình, rồi `idf.py build`. File app là `build/esp32_hybrid_minicar.bin`.
5. Ngắt tải truyền động/đánh lửa, cấp riêng DevKit qua USB. Chọn cổng COM thật của board, chạy `idf.py -p COMx flash monitor` trong `firmware/esp-idf`.
6. Monitor 115200; Ctrl+] để thoát. Đọc dòng `PHONE: WiFi ESP32-Hybrid | password ...`. Mật khẩu đổi sau reset.

Ví dụ từ ESP-IDF Terminal, khi đang ở root repo:

```powershell
Set-Location firmware/esp-idf
idf.py build
idf.py -p COMx flash monitor
```

COMx là placeholder, phải thay bằng cổng thật. Terminal PowerShell chưa kích hoạt SDK có thể không tìm thấy idf.py. Không chép `build/` hoặc `sdkconfig` cũ từ máy khác vào bản download mới.

## Thử tín hiệu trước khi nối tải

- Đấu LED, E-Stop và biến trở theo [PINOUT](PINOUT.md). BAT SIM phải đủ 10.2 V.
- Quan sát GPIO18/19/23/26 bằng analyzer có GND tham chiếu. D0/D1 trong Wokwi là tên kênh analyzer, không phải GPIO0/1.
- ARM → RUN. Bản C timeout 500 ms nên gõ tay thường sẽ timeout: dùng phone UI hoặc bộ gửi PING mỗi 100 ms.
- Đo xung từ lúc boot/reset; trạng thái chân trước board_init và hành vi ESC khi mất PWM chưa được app bảo đảm.
- Chỉ ghép servo sau khi đo nguồn ngoài, kiểm tra connector, tháo linkage để hiệu chuẩn ga đóng.

| File | Trách nhiệm |
|---|---|
| `main.c` | Một vòng điều khiển, watchdog 2 s, yield 1 tick |
| `controller.c` | FSM, parser, owner, timeout 500 ms, interlock |
| `board.c` | ADC1, GPIO, LEDC, UART không chờ TX, chốt cạnh E-Stop |
| `phone.c` | SoftAP/HTTP, queue 8 lệnh, bit STOP, snapshot trạng thái |
| `web.html` | ARM, giữ RUN, nhả/ẩn trang thì STOP |
| `vehicle_config.h` | GPIO, thời gian, dải xung thử |

LEDC phát xung bằng phần cứng ở 50 Hz; chỉ main task cập nhật đích. Không tạo một task riêng cho mỗi servo/ESC. Tick 1 ms không bảo đảm mỗi vòng luôn hoàn tất trong 1 ms.

## Arduino/Wokwi

Ba file nhập Wokwi ở `firmware/wokwi/`: `sketch.ino`, `diagram.json`, `libraries.txt`. ESP32Servo 3.2.1, Arduino core 3.3.12 và CLI 1.5.1. `scripts/build.ps1` hoặc `scripts/build.sh` tạo staging hợp lệ. Bản này giữ 5 giây, không chứa phone UI của bản C.
