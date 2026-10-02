# Chuẩn bị thử bàn trên ESP32-WROOM

Target build là **ESP32 Dev Module** (`esp32:esp32:esp32`), phù hợp bước thử với DevKit dùng ESP32-WROOM cổ điển. Xác nhận model/nhãn GPIO của board đang có; đế mở rộng 38 chân không tự chứng minh board và đế khớp nhau. Không dùng số thứ tự chân trên module làm số GPIO.

## Nạp và thử tín hiệu trước

1. Dùng USB cấp nguồn cho riêng DevKit ở bước thử tín hiệu. Build theo README, chọn đúng cổng Serial trong Arduino IDE/CLI.
2. Giữ pin map 18/19/23/26/27/34/25/32/33. Đấu E-Stop GPIO27–GND; LED với điện trở 220–330 Ω. Xác nhận tín hiệu trên analyzer/oscilloscope trước khi ghép tải.
3. Dùng biến trở 3V3–GND, SIG GPIO34 để thử logic. BAT lúc này vẫn được ghi SIM; không coi là đo pin thật, kể cả chạy trên ESP32 thật.
4. Nạp và reset, quan sát các xung từ lúc boot. Kiểm tra đủ ARM/RUN, STOP, giữ E-Stop, timeout, lỗi dòng Serial. Không flash/nối công suất tự động từ các script trong repo.

Đây là sketch Arduino chạy trên Arduino-ESP32. Google Doc có hướng ESP-IDF, nhưng chưa có bản port ESP-IDF thuần trong repo. Không mở trực tiếp sketch bằng idf.py và coi nó là project IDF.

## Khi chuyển sang LiPo và tải thật

| Phần | Việc phải xác định trước khi sử dụng |
|---|---|
| Đo LiPo 3S | Thêm cầu chia áp/lọc/bảo vệ phù hợp cho GPIO34; chọn tỷ lệ theo điện áp cực đại; đo với đồng hồ và dùng ADC đã hiệu chuẩn. Thay công thức Wokwi trong sampleBattery; xác định ngưỡng cắt theo pack/ESC. Chưa lắp mạch đo thì firmware không giám sát được pin thật. |
| Servo | Nguồn ngoài đúng điện áp/dòng theo model và dòng stall của cả hai servo, GND tham chiếu đúng. Hiệu chuẩn góc đóng ga/hành trình; giá trị 0° không tự bảo đảm bướm ga thật đóng. |
| ESC | Xác nhận nhận logic 3.3 V, chu kỳ/xung stop, arming, giới hạn ga và hành vi mất xung. 1000–1600 µs đang là dải test; không khẳng định đúng mọi ESC. |
| RCEXL | Xác nhận model, chân nguồn/tín hiệu, điện áp, loại ignition và hành vi mất PWM. GPIO26 1000/1500 µs chỉ là marker thử; chưa cho phép suy ra ignition đã OFF/ON. |
| Dừng độc lập | E-Stop trong sketch là ngõ vào phần mềm. MCU treo/mất nguồn có thể không thực hiện safeOutputs; cần cơ chế kill phù hợp với động cơ và hồi ga đã thử. |

Trong mô phỏng, servo nối 5V board là cách nuôi phần tử ảo. Không dùng sơ đồ đó để cấp trực tiếp hai servo tải lớn từ rail của ESP32. Theo [hướng dẫn DevKitC V4](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/esp32-devkitc/user_guide.html), chỉ dùng một trong các đường cấp board: USB, 5V hoặc 3V3. Không nối đầu ra buck song song BEC của ESC.

GPIO34 là ADC1, chỉ dùng đầu vào. [Tài liệu ADC Arduino-ESP32](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/adc.html) phân biệt raw chưa hiệu chuẩn và analogReadMilliVolts; tại attenuation 11 dB, dải đo ESP32 được nêu khoảng 150–3100 mV. Vì vậy phép tuyến tính raw→12.6 V hiện tại không phải hiệu chuẩn đo LiPo. Không nối 3S hoặc 5V trực tiếp vào GPIO34.

Các giá trị trong tài liệu nguồn còn cần đối chiếu: motor ghi cả 1250 KV/1450 KV; servo ghi 6.6–7.4 V; RCEXL K1 chưa có manual đúng model; phương án đánh lửa DC/magneto chưa chốt. Không bổ sung relay hay đổi chân từ các giả định đó.
