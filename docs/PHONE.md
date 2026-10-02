# Điện thoại — trình duyệt qua Wi-Fi

Bản **ESP-IDF C** có web UI trong ESP32, không cần app bên thứ ba/Internet. Wokwi Arduino chỉ có Serial giả lập.

1. Flash bản C, mở USB Serial 115200 và reset.
2. Điện thoại nối Wi-Fi **ESP32-Hybrid**, mật khẩu ở dòng `PHONE`. Mật khẩu đổi mỗi lần boot; AP chỉ nhận một thiết bị.
3. Giữ mạng Wi-Fi không Internet, mở **http://192.168.4.1**.
4. Thả E-Stop, BAT SIM đủ mức; nhấn ARM, chờ ARMED.
5. **Giữ RUN**, dùng ngón khác chỉnh lái/ga/ESC khi RUNNING. Nhả RUN hoặc STOP để dừng. Tab ẩn/mất focus cũng yêu cầu STOP.
6. Sau failsafe phải ARM/RUN lại; UI không tự ARM lại.

Khi giữ RUN và còn trạng thái mới, UI gửi PING mỗi **100 ms**. GET trạng thái mỗi 150 ms không duy trì heartbeat. Server timeout **500 ms**; Wi-Fi disconnect cũng yêu cầu STOP. Lệnh UI đợi >150 ms bị hủy, lệnh nằm queue firmware >100 ms bị từ chối. Trình duyệt có thể bị hệ điều hành trì hoãn; phải thử khóa màn hình/chuyển app/mất sóng theo [bảng test](HARDWARE_TESTS.md).

## Giao thức

`POST /api/command`, body ASCII như `ARM`, `ESC 20`, tối đa 47 ký tự. Headers:

- `X-Control-Session`: uint32 khác 0 ngẫu nhiên theo lần mở trang.
- `X-Control-Seq`: uint32 khác 0 tăng dần; lệnh lặp/cũ không gia hạn timeout.
- `X-Control-Epoch`: lấy từ `/api/status`, đổi mỗi STOP; chặn lệnh cũ tái chạy sau dừng.

202 chỉ nghĩa đã xếp yêu cầu. `/api/status` trả FSM, owner, epoch, đích đầu ra, BAT SIM, lý do dừng và acknowledgement (`ack_session`, `ack_seq`, `accepted`). STOP chính xác chữ hoa đi qua bit sự kiện, không đợi queue trống.

Chỉ nguồn đã ARM được RUN/điều khiển/PING/BAT khi ARMED/RUNNING. Serial source 0; web khác 0. Mọi nguồn được STOP. WPA2 bảo vệ liên kết; session/sequence tránh nhầm nguồn/lặp lệnh, không phải chứng nhận an toàn/danh tính người vận hành.

Chưa xác nhận kết nối điện thoại thật trong lần sửa này. UI RUNNING không chứng minh động cơ chạy, STOPPED không chứng minh ignition tắt.
