# Review đầu vào trước khi sửa — 02/10/2026

Đã đọc toàn bộ sketch.ino, diagram.json, libraries.txt trong ESP-MiniCar.zip; đọc nhãn GraphML, nội dung HTML và hướng dẫn của bộ 12 sơ đồ; xem ảnh tổng quát, nguồn, GPIO, FSM và RCEXL. Đã đọc các tab của Google Doc EES-Minicar được người dùng gửi. Tài liệu là nguồn tham khảo; yêu cầu trực tiếp của người dùng quyết định phạm vi firmware.

| Lỗi/rủi ro ban đầu | Cách xử lý |
|---|---|
| E-Stop chỉ bắt cạnh; giữ nút rồi ARM/RUN có thể chạy lại | Kiểm tra mức LOW liên tục, khóa cả ARM và RUN |
| readStringUntil có thể chờ, làm trễ safety | Bộ đọc từng byte có giới hạn công việc mỗi vòng |
| toInt nhận dữ liệu không hợp lệ thành 0 hoặc tiền tố số | Kiểm tra toàn bộ chuỗi số, giới hạn độ dài/phạm vi |
| BAT hợp lệ không refresh heartbeat | Tất cả lệnh được chấp nhận khi RUNNING đều refresh |
| Xử lý lệnh trước timeout; PING muộn có thể che lỗi | Kiểm tra safety trước đọc và trước xử lý frame; timeout ≥ 5000 ms |
| ARM không khóa pin yếu/E-Stop; ARM lúc đang RUN đổi state ngầm | Chuyển trạng thái có điều kiện; pin yếu khóa từ STOPPED/ARMED/RUNNING |
| Lệnh chờ trong RX có thể tái khởi động sau STOP | Bỏ RX đang chờ và phần dòng dở khi dừng |
| Log Serial có thể chặn nếu TX đầy | Chỉ ghi khi đủ chỗ; bỏ log dưới tải cao |
| diagram: pot1 VCC nối 5V | Sửa dây sang 3V3; GPIO34 giữ nguyên |
| diagram: GPIO25 ra LED vàng, GPIO33 ra LED xanh | Hoán đổi hai đầu dây đến r2/r3 cho đúng màu; không đổi GPIO firmware |
| GPIO26 có thể bị hiểu thành kill thật | Đổi tên/comment thành RCEXL test; nêu rõ chưa xác nhận ON/OFF |
| Không kiểm tra PWM attach thành công | Không ARM/RUN nếu một trong bốn kênh chưa attach |
| Thư viện không khóa phiên bản | ESP32Servo@3.2.1; CI khóa core và CLI |

Các quyết định giữ nguyên: FSM ba trạng thái; timeout 5 giây; ngưỡng pin mô phỏng 10.2 V; ESC test tối đa 1600 µs; ga tối đa 135°; toàn bộ GPIO theo yêu cầu. Thay đổi tiện dùng: pot1 bắt đầu ở 1023 để BAT ở khoảng 12.6 V; hệ thống vẫn boot STOPPED.

Các điểm không lấy từ sơ đồ tham khảo làm yêu cầu mới:

- H05 dành GPIO25 cho RUN_ALLOW và GPIO32/33/27 cho receiver tùy chọn. Firmware này dùng LED và E-Stop đúng mô tả trực tiếp của người dùng.
- H06 đề xuất FSM nhiều trạng thái và timeout 300 ms. Firmware này giữ STOPPED/ARMED/RUNNING và 5000 ms.
- Google Doc có ghi ESP-IDF, UART điện thoại và hai cấu hình pin 2S/3S. Bản sửa hiện tại tiếp tục sketch Arduino/Wokwi và Serial giả lập; không tự chuyển sang ESP-IDF thuần hay thêm BLE.
- Không suy ra RCEXL K1 giống V2.0, không chốt đánh lửa DC hay magneto, không coi trạng thái phần mềm STOPPED chứng minh động cơ thật đã tắt.

Bản gốc nằm ở `references/original-wokwi/` trên máy làm việc. Các file tham khảo được lưu riêng tại máy, không đưa vào repo công khai.
