# About — phạm vi và mức xác nhận

Project điều khiển thử xe hybrid mini: hai servo lái/ga xăng, ESC BLDC, E-Stop, báo trạng thái và điện áp pin mô phỏng. Người dùng chọn C/ESP-IDF, điện thoại và timeout thử bàn 500 ms. Wokwi giữ yêu cầu gốc 5 giây.

## Vì sao dùng FreeRTOS?

ESP-IDF khởi động scheduler và gọi `app_main` trong main task; Wi-Fi/HTTP cũng có task của SDK. Project không tự thêm bốn luồng điều khiển cơ cấu. [Espressif mô tả cơ chế này](https://docs.espressif.com/projects/esp-idf/en/release-v5.5/esp32/api-reference/system/freertos.html); API đã đối chiếu với SDK 6.0.2 cài tại máy.

**Chỉ main task gọi FSM và ghi đích PWM.** HTTP đưa lệnh vào queue; Wi-Fi callback báo sự kiện; ISR E-Stop chỉ đặt cờ. HTTP đọc bản sao trạng thái có khóa ngắn. Cách này tránh cập nhật ga/ESC từ nhiều task, nhưng không biến Wi-Fi hay FreeRTOS thành hệ thống đã chứng nhận an toàn.

| Cơ chế | Đã triển khai | Giới hạn |
|---|---|---|
| Timeout | ≥500 ms từ lệnh hợp lệ cuối khi RUNNING; PING muộn không cứu timeout | Còn chờ task chạy, chu kỳ PWM và đáp ứng cơ khí |
| E-Stop | Mức LOW + chốt cạnh xuống; nhả không tự chạy | Chỉ chốt cạnh ISR quan sát được; MCU treo/mất điện cần kill độc lập |
| Watchdog | Theo dõi task điều khiển, 2 s, panic/reset | Khác timeout 500 ms; LEDC có thể phát xung cũ tới reset nếu task treo |
| Serial | Parser chặt, 32 byte/lần, log TX không chờ | ASCII không CRC/xác thực; dùng USB thử bàn, chưa là bus chống nhiễu |
| Điện thoại | Một owner, sequence tăng, epoch đổi khi STOP; loại lệnh nằm queue >100 ms | Cần đo mất gói/độ trễ thực |
| Pin thấp | ADC raw → BAT SIM, <10.2 V thì dừng | Chưa đo pin thật, chưa hiệu chuẩn cầu chia áp |
| RCEXL | Test 1000/1500 µs GPIO26 | Chưa chứng minh ngắt đánh lửa |

## Đã kiểm tra trên máy

- C native: **7422 phép kiểm tra** (gồm vòng lặp dữ liệu/PWM), không phải 7422 tình huống phần cứng.
- Arduino/Wokwi: **453 assertions** trên sketch thật với GPIO/ADC/Servo/Serial giả lập.
- Logic JavaScript giao diện và đối chiếu GPIO hai firmware.
- ESP-IDF 6.0.2 build ESP32; Arduino ESP32 build CI. Xem kết quả từng commit ở GitHub Actions.

Chưa flash/đo board thật, chưa thử điện thoại nối SoftAP thật, chưa đo nhiễu khi đánh lửa, chưa ghi VCD Wokwi tương tác cho bộ test. Không đánh dấu đủ điều kiện thi từ các kiểm tra phần mềm này.

## Phần cứng còn thiếu dữ liệu

Ảnh ghi servo >15 kg·cm, **6.0–7.4 V**, chưa có model/dòng stall. Pin 3S 2200 mAh 45C. Buck quảng cáo 300 W/20 A (liên tục 15 A), chưa đo tải/nhiệt. Motor có thông tin mâu thuẫn 1250/1450 KV. Relay có biến thể 5/12/24 V, High/Low; chưa biết board thật. Nhãn 30 A chưa chứng minh khả năng cắt tải DC/cảm ứng cụ thể.

Đã biết động cơ xăng 2 kỳ có đánh lửa; thiếu loại DC/magneto, model ESC và RCEXL. Repo chốt dây tín hiệu đã biết, đánh dấu nguồn/kill cần manual; không tự nối relay hay GPIO26 vào đánh lửa.
