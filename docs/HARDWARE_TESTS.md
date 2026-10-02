# Kiểm thử trước khi cho xe chuyển động

**Chưa thực hiện trên xe thật.** Thử từng mức tải trên giá thử phù hợp, có người vận hành kill độc lập. Ghi model, dây nguồn, commit, điện thoại, khoảng cách và tải. Không tự tạo tia lửa cao áp/chập nguồn để thử nhiễu.

Timeout logic 500 ms còn cộng chờ task, chu kỳ PWM 20 ms và đáp ứng cơ khí. Chưa đo cực đại, **không cam kết xe dừng trong 500 ms**. Watchdog 2 s là cơ chế riêng khi task treo.

| Ca | Thao tác / số đo | Kết quả cần đạt |
|---|---|---|
| Boot/reset | Analyzer 18/19/23/26 từ lúc cấp nguồn | Không có đích ga ngoài ý muốn; đo cả giai đoạn trước app |
| FSM | RUN chưa ARM; điều khiển lúc STOPPED/ARMED | Từ chối, giữ idle |
| Timeout | Lệnh hợp lệ mỗi 100 ms rồi ngừng, ghi mốc cuối và PWM | Logic ≥500 ms; đo độ trễ đầu ra, không tự chạy lại |
| Lệnh rác | Sai cú pháp, dòng tràn/binary/PING lặp sequence | Không gia hạn heartbeat/đổi đích |
| E-Stop | Nhấn giữ khi có ga; ARM/RUN khi giữ; nhả | Dừng; giữ chặn ARM/RUN; nhả không tự chạy |
| E-Stop ngắn | Bộ phát thử mức 3.3 V đúng giới hạn | Ghi xung ngắn nhất bắt được và trễ dừng |
| Đứt dây E-Stop | Chỉ thử bàn không công suất | Ghi hạn chế nút NO/pull-up: không bảo đảm phát hiện |
| BAT SIM | Qua ngưỡng 10.2 V, dao động quanh ngưỡng | Dừng khi mẫu thấp; hồi cao không tự chạy; chưa lọc trì hoãn mẫu thấp |
| Phone | Nhả RUN, pointer cancel, chuyển app, khóa màn hình | STOP/timeout, không phát lại ga cũ |
| Mất Wi-Fi | Tắt Wi-Fi/ra vùng phủ | Disconnect hoặc timeout; không tự ARM |
| HTTP chậm | Trì hoãn GET/POST có kiểm soát | Ngừng PING khi trạng thái cũ; timeout; queue không phát vô hạn |
| Hai nguồn | Phone ARM, Serial ESC/PING/STOP và ngược lại | ESC/PING từ nguồn khác bị từ chối; STOP có hiệu lực |
| Treo task | Bản thử riêng cách ly tải, watchdog/reset | Ghi PWM và ESC/ignition thực; reset không tự đồng nghĩa kill |
| Nguồn servo | Hai servo di chuyển với tải cho phép; đo Vservo/5V/3V3 tại tải | Không brownout/reset/ga ngoài ý muốn; xác minh nguồn và dây |
| Nhiễu BLDC | Tăng tải trong giới hạn thiết bị | Ghi PWM/nguồn/mất gói/reset; không chuyển động ngoài ý muốn |
| Nhiễu ignition | Sau khi chốt manual/mạch kill, vận hành bình thường có kiểm soát | Ghi như trên; kill tác dụng độc lập MCU/Wi-Fi |
| MCU mất nguồn | Mô hình thử an toàn | ESC/ignition/hồi ga về an toàn bằng phần cứng đã xác minh |

Đích dừng firmware: lái 90°, ga 0°, ESC 1000 µs, GPIO26 test 1000 µs, LED đỏ. Đo riêng thời gian ga đóng/động cơ ngừng lực. GPIO26 test không chứng minh ignition đã tắt.

| Commit / ca | Model & nguồn | Tải / vị trí dây | Max trễ PWM / nguồn min-max | Mất gói / reset | PASS/FAIL / file đo |
|---|---|---|---|---|---|
| Chưa đo | | | | | |

Khi BAT còn SIM, RCEXL còn test, kill độc lập chưa chốt thì hệ thống vẫn là bản thử bàn. Cần manual ESC/servo/ignition và đo EMI toàn xe trước khi chốt cấu hình thi.
