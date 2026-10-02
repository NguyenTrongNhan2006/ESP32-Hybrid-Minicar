# Các test cần chạy trong Wokwi

Trạng thái xác nhận: host tests kiểm tra logic; GitHub Actions kiểm tra biên dịch ESP32. Các ca mô phỏng tương tác và đo xung bên dưới **chưa được chạy/ghi VCD trong lần cập nhật này**. Không dùng kết quả host tests thay cho đo PWM.

Chuẩn bị: dùng ba file ở `firmware/wokwi/`; Serial 115200, newline; biến trở ở mức cao, BAT khoảng 12.6 V; bật ghi Logic Analyzer. Gửi từng lệnh, khoảng cách dưới 5 giây. Sau mỗi STOP/failsafe, đợi thông báo rồi ARM/RUN mới. Nếu trước đó gửi dở dòng, gửi newline trống trước. Bản C riêng dùng 500 ms, xem HARDWARE_TESTS.md.

Mẫu an toàn **S** dùng trong bảng: STOPPED; lái 90°; ga 0°; D0 khoảng 1000 µs; D1 test khoảng 1000 µs; LED đỏ sáng, xanh/vàng tắt. D0/D1 có chu kỳ khoảng 20 ms. Cho phép sai số lượng tử PWM nhỏ; đo bằng VCD/Logic Analyzer, không dựa vào góc hiển thị để suy ra xung ESC.

| # | Thao tác | Kết quả cần quan sát |
|---:|---|---|
| 1 | Start/reset, không nhập lệnh | S; không tự ARM/RUN |
| 2 | Gửi RUN, GAS 100, ESC 100, STEER 45 khi STOPPED | Từ chối; giữ S |
| 3 | ARM | ARMED, chỉ LED xanh; đầu ra giữ idle, D1 test 1000 µs |
| 4 | GAS/ESC/STEER khi ARMED | Từ chối; ga/ESC vẫn 0; lái 90° |
| 5 | RUN sau ARM | RUNNING, chỉ LED vàng; ga/ESC vẫn 0; D1 test 1500 µs |
| 6 | STEER 45 rồi 135; GAS 0 rồi 100; ESC 0, 50, 100 | Lái hai giới hạn; ga 0/135°; D0 1000/1300/1600 µs |
| 7 | PING mỗi 1 giây trong >10 giây | Giữ RUNNING |
| 8 | BAT mỗi 1 giây trong >10 giây | Giữ RUNNING; BAT cũng là heartbeat hợp lệ |
| 9 | RUN, tăng ga/ESC/lệch lái; ngừng mọi lệnh | Sau 5 giây từ lệnh hợp lệ cuối, về S; LAST_STOP=TIMEOUT |
| 10 | Khi RUNNING, chỉ gửi GAS abc, GAS 50xyz, ESC -1, ESC 101, STEER 44, PING junk | Mỗi lệnh bị từ chối; không kéo dài timeout; về S đúng hạn |
| 11 | Khi RUNNING, gửi ARM hoặc RUN lần nữa | Bị từ chối, không đổi đầu ra và không refresh timeout |
| 12 | Khi có ga/ESC, nhấn và giữ E-Stop; vẫn giữ rồi ARM/RUN | Về S; ARM/RUN không vượt được E-Stop |
| 13 | Thả E-Stop, gửi PING hoặc RUN | Vẫn S; phải ARM rồi RUN mới được chạy, ga/ESC khởi đầu 0 |
| 14 | Start/reset khi đang giữ E-Stop | S, ARM/RUN bị khóa cho tới khi thả và gửi lệnh mới |
| 15 | Khi có ga/ESC, hạ biến trở để BAT <10.2 V | Về S, LAST_STOP=LOW_BATTERY; lấy mẫu mỗi 20 ms |
| 16 | Giữ điện áp thấp, ARM/RUN; sau đó tăng lại | Pin thấp chặn ARM/RUN; tăng lại không tự chạy; cần ARM/RUN mới |
| 17 | ARM khi pin cao rồi hạ biến trở trước RUN | Về S; không RUN được với pin thấp |
| 18 | Khởi động với biến trở thấp | S; ARM bị từ chối |
| 19 | RUN, có ga/ESC/lệch lái, STOP | Về S ngay khi nhận trọn dòng STOP |
| 20 | RUN, dùng terminal gửi ESC 100 nhưng chưa newline; nhấn E-Stop hoặc đợi timeout | Dòng chưa hoàn chỉnh không thi hành; vẫn về S; không chờ Serial timeout |
| 21 | Gửi dòng >47 ký tự, kết thúc bằng ESC 100 rồi newline | Bỏ cả dòng, không thực thi phần đuôi; dòng mới hợp lệ hoạt động |
| 22 | Gửi lệnh chữ thường, nhiều space/tab, thử LF/CR/CRLF | Cùng hành vi với lệnh chuẩn, không thực thi hai lần |
| 23 | Gửi STOP + ARM + RUN cùng một burst | STOP chốt S; bỏ các byte đã chờ trong RX, gửi ARM/RUN mới sau đó |
| 24 | Sau timeout, gửi PING/GAS/ESC/RUN mà chưa ARM | Không khôi phục chuyển động; S |
| 25 | Quan sát boot, STOP, E-Stop và timeout trên cả D0/D1 | Ghi độ rộng xung và thời điểm chuyển về mẫu S |

Biên đúng 4999/5000 ms, tràn millis và TX/RX flood đã có host test tự động; thao tác tay không đủ chính xác để chứng minh biên thời gian. Khi cần, dùng bộ phát Serial/automation có timestamp và VCD. Lệnh được chấp nhận tính tại lúc firmware nhận xong dòng, không phải lúc bắt đầu gõ.

BAT dùng ADC raw 0..4095 quy đổi 0..12.6 V; biên raw 3315 tương ứng 10.200 V, raw 3314 thấp hơn ngưỡng. Dùng điện áp BAT thay vì ước lượng bằng vị trí núm. Wokwi không mô phỏng đầy đủ sai số ADC, nguồn servo, dòng motor, tiếp điểm hay khối RCEXL thật.

Biểu mẫu ghi kết quả:

| Test | PASS/FAIL | Giá trị/độ trễ quan sát | Tên VCD/ảnh | Ngày/người thử |
|---|---|---|---|---|
| | | | | |

Nguồn cách ghi tín hiệu: [Wokwi Logic Analyzer](https://docs.wokwi.com/guides/logic-analyzer). GPIO26 chỉ dùng xác nhận FSM đổi **xung test**, không đánh dấu test này là RCEXL thật ON/OFF thành công.
