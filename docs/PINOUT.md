# Pinout và đấu dây thử bàn

Giữ nguyên GPIO đã thống nhất. Dùng tên GPIO in trên DevKit, không dùng số thứ tự chân WROOM/đế 38 chân làm số GPIO.

| Chức năng | GPIO | Nối đến | Ghi chú |
|---|---:|---|---|
| Servo lái | 18 | Signal servo lái | Nguồn ngoài phù hợp |
| Servo ga xăng | 19 | Signal servo ga | 0° chưa chứng minh bướm ga đóng |
| ESC BLDC | 23 | Analyzer D0 trước; sau xác minh mới nối signal ESC | Test 50 Hz, 1000–1600 µs; idle 1000 µs |
| RCEXL test | 26 | Analyzer D1 | Test 1000/1500 µs; chưa nối ignition |
| E-Stop | 27 | Nút thường hở về GND | INPUT_PULLUP; LOW = dừng |
| BAT SIM | 34 | SIG/wiper biến trở | Hai đầu biến trở 3V3/GND; ADC1 channel 6 |
| LED xanh ARMED | 25 | 220–330 Ω → anode; cathode → GND | Sáng ở ARMED |
| LED đỏ STOPPED | 32 | 220–330 Ω → anode; cathode → GND | Sáng khi STOPPED/failsafe |
| LED vàng RUNNING | 33 | 220–330 Ω → anode; cathode → GND | Sáng ở RUNNING |
| USB Serial | UART0 GPIO1/3 | USB bridge có sẵn | 115200; không thêm tải |

## Mỗi servo có ba dây

1. **Signal** → GPIO18 hoặc GPIO19.
2. **V+** → đầu dương buck/BEC riêng đúng điện áp servo. Ảnh ghi 6.0–7.4 V; đối chiếu nhãn/manual thật trước khi chỉnh buck.
3. **GND** → âm nguồn servo, có tham chiếu về GND điều khiển tại điểm phân phối phù hợp. Dòng lớn servo về nguồn qua dây nguồn của nó, không đi xuyên dây GND nhỏ/board ESP32.

Màu thường gặp: đỏ V+, nâu/đen GND, cam/vàng/trắng signal; **không xác định pinout chỉ bằng màu**. Kiểm tra connector/manual. Đo nguồn trước khi cắm. Không cấp LiPo 3S trực tiếp, không nuôi hai servo tải lớn từ 3V3/5V DevKit. Dòng stall cả hai servo và sụt áp thực quyết định nguồn, không chỉ nhãn 20 A trên buck.

Lần đầu tháo linkage/đòn truyền và hạn chế hành trình. Xung ga 0° hiện là **500 µs**, có thể ngoài dải servo thật; hiệu chuẩn giới hạn trước khi kéo cơ cấu. Cần hồi ga cơ khí phù hợp khi mất nguồn.

## Nguồn ESP32, ESC, pin và đánh lửa

- Thử tín hiệu: cấp DevKit bằng USB. Khi lắp xe có thể dùng buck 5 V riêng vào 5V/GND của **đúng board hỗ trợ**, sau khi đối chiếu board/đo nguồn. Không song song USB/5V/3V3; [DevKitC V4 quy định chọn một đường cấp](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/esp32-devkitc/user_guide.html).
- ESC: xác minh connector signal/GND, mức 3.3 V, BEC, dải xung và mất tín hiệu. Không chập đầu dương BEC với đầu ra buck khác. Chọn một nguồn servo, cách điện đầu dương nguồn còn lại theo thiết kế.
- 3S đầy có thể 12.6 V. GPIO34 hiện **chỉ nối biến trở**, không nối pack/5 V trực tiếp. Đo pin thật cần cầu chia áp, lọc/bảo vệ, ADC hiệu chuẩn và thay phép quy đổi. Hiện BAT không bảo vệ pin thật.
- RCEXL/relay chưa chốt nguồn, pinout, logic, kill. Không cấp coil relay từ GPIO. Không nối tắt đầu đánh lửa xuống GND nếu chưa rõ DC/magneto và hướng dẫn hãng.
- Cần mạch dừng độc lập MCU. GPIO27 là E-Stop phần mềm; nút thường hở có thể không phát hiện đứt dây. Chưa tự đổi chân hoặc đổi loại tiếp điểm trong firmware.

## Khi nhiễu cao

Đặt ESP32/antenna và dây tín hiệu xa dây bugi/cao áp, cuộn đánh lửa, pha motor và buck. Đi signal cùng dây GND tham chiếu, giữ ngắn; tránh vòng dây lớn và dòng công suất chạy qua GND tín hiệu. GND/cách ly ignition theo đúng thiết bị, không nối tắt phần cách ly quang.

Chọn lọc nguồn, decoupling, bảo vệ và giao tiếp chịu nhiễu bằng sơ đồ/phép đo tại tải. Chưa chốt trị số RC tùy ý trên PWM/E-Stop; lọc quá mạnh có thể méo xung/trễ dừng. [Hướng dẫn Espressif](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32/pcb-layout-design.html) nhấn mạnh nguồn, đường hồi GND và khoảng trống antenna; không thay cho đo EMI toàn xe.
