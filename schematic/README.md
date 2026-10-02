# Sơ đồ tự tạo theo firmware

Mermaid hiển thị trực tiếp trên GitHub. Đây là kiến trúc, FSM và luồng xử lý; [PINOUT](../docs/PINOUT.md) là bảng đấu tín hiệu. Phần nguồn/ignition thiếu model được đánh dấu, chưa là sơ đồ nguyên lý đã duyệt cho xe thi.

## FSM

```mermaid
stateDiagram-v2
    [*] --> STOPPED: Boot / reset
    STOPPED --> ARMED: ARM + interlock OK
    ARMED --> RUNNING: RUN từ nguồn đã ARM
    ARMED --> STOPPED: STOP / E-Stop / pin thấp / lỗi
    RUNNING --> STOPPED: STOP / E-Stop / pin thấp / lỗi / timeout
    RUNNING --> RUNNING: Lệnh hợp lệ từ owner cập nhật heartbeat
    STOPPED --> STOPPED: PING không tự chạy lại
    note right of STOPPED
        Ga 0 độ, lái 90 độ
        ESC 1000 us, GPIO26 test 1000 us
        LED đỏ
    end note
    note right of ARMED
        Đầu ra idle, LED xanh
    end note
    note right of RUNNING
        Cho phép STEER / GAS / ESC
        GPIO26 test 1500 us, LED vàng
        IDF 500 ms; Wokwi 5000 ms
    end note
```

## Flowchart

```mermaid
flowchart TD
    A[Boot: GPIO / ADC / LEDC ở đích dừng] --> B[STOPPED - Wi-Fi và watchdog]
    B --> C[Kiểm tra E-Stop, ADC, pin, driver, timeout]
    C --> D{Có lỗi hoặc hết hạn?}
    D -- Có --> S[STOPPED, đích dừng, LED đỏ, tăng epoch, bỏ lệnh cũ]
    D -- Không --> E[Serial tối đa 32 byte, không chờ dòng]
    S --> E
    E --> F[Bit STOP / mất Wi-Fi trước queue]
    F --> G[Tối đa 4 lệnh web]
    G --> H{Epoch / tuổi / seq / owner hợp lệ?}
    H -- Không --> I[Từ chối, không gia hạn heartbeat]
    H -- Có --> J[Safety lại, parse, chuyển FSM / cập nhật đích]
    I --> K[Safety và Serial lần nữa]
    J --> K
    K --> L[TX không chờ, reset watchdog, yield 1 tick]
    L --> C
```

Tick 1 ms không bảo đảm vòng lặp hoàn tất trong 1 ms. Chỉ main task ghi FSM/PWM; LEDC tạo xung bằng phần cứng.

```mermaid
flowchart LR
    UI[Điện thoại] --> HTTP[Task HTTP của SDK]
    HTTP --> Q[Queue 8 lệnh + timestamp / epoch / seq]
    HTTP --> EV[Bit STOP]
    WIFI[Wi-Fi event callback] --> EV
    ISR[ISR GPIO27 chỉ chốt cạnh] --> FLAG[Cờ E-Stop]
    Q --> MAIN[Main task: FSM + safety]
    EV --> MAIN
    FLAG --> MAIN
    UART[USB Serial bounded RX] --> MAIN
    MAIN --> PWM[LEDC PWM 18 / 19 / 23 / 26]
    MAIN --> SNAP[Snapshot có khóa ngắn]
    SNAP --> HTTP
```

## Toàn hệ thống

```mermaid
flowchart LR
    PHONE[Điện thoại] <-->|Wi-Fi SoftAP| ESP[ESP32-WROOM DevKit]
    USB[USB - nguồn ESP32 khi thử bàn] --> ESP
    BAT[LiPo 3S - tối đa 12.6 V] --> DIST[Cầu chì / phân phối theo tải thực]
    DIST --> BUCK[Buck servo chỉnh theo model]
    BUCK --> SV1[Servo lái]
    BUCK --> SV2[Servo ga xăng]
    ESP -->|GPIO18 signal| SV1
    ESP -->|GPIO19 signal| SV2
    DIST -. Sau xác minh nguồn .-> ESC[ESC - model chưa chốt]
    ESP -. GPIO23 cần xác minh mức logic và xung .-> ESC
    ESC --> MOTOR[BLDC]
    ESP -->|GPIO23| D0[Analyzer D0]
    ESP -->|GPIO26 test ONLY| D1[Analyzer D1]
    ESTOP[Nút GPIO27 - GND] --> ESP
    POT[Biến trở 3V3 / GND / SIG34] --> ESP
    ESP --> LED[LED xanh25 / đỏ32 / vàng33 + điện trở]
    KILL[Kill độc lập chưa chốt theo ignition] -. Thiết kế và xác minh .-> IGN[RCEXL / đánh lửa 2 kỳ]
    IGN --> ICE[Động cơ xăng]
    SV2 -->|Ga cần hiệu chuẩn| ICE
```

GND tín hiệu ESP32/servo/ESC/analyzer cần tham chiếu đúng; dòng servo/motor đi riêng về phân phối. Không nối đầu ra buck/BEC song song. Không nối GPIO26 vào ignition chỉ vì thấy xung analyzer. Chưa gán GPIO cho relay/kill, chưa dùng GPIO34 đo pin thật. Nguồn/cách ly ignition theo manual đúng model.
