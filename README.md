# Digital Clock - Group 1 🕐

Dự án đồng hồ kỹ thuật số sử dụng Arduino Uno R3 với các tính năng: hiển thị giờ/ngày, chỉnh giờ bằng nút nhấn, và báo thức.

## Thành viên nhóm
| # | Họ tên | Vai trò |
|:---:|--------|---------|
| 1 | Nguyễn Trí Thiện | Trưởng nhóm - Kiến trúc & Tích hợp |
| 2 | Nguyễn Văn An | Chuyên gia RTC (Thời gian thực) |
| 3 | Hàng Võ Minh Nhật | Chuyên gia LCD (Hiển thị) |
| 4 | Trần Công Tiến | Chuyên gia Nút nhấn (Tương tác) |
| 5 | Văn Thái Trung | Chuyên gia Báo thức & QA |

> **Lưu ý**: Thứ tự vai trò có thể thay đổi tùy theo phân công thực tế của nhóm.

## Tính năng
- ⏰ Hiển thị **Giờ:Phút:Giây** và **Ngày/Tháng/Năm** trên LCD 16x2
- 🔧 Chỉnh giờ/phút bằng 3 nút nhấn (MODE, UP, DOWN)
- 🔔 Đặt báo thức (giờ + phút), bật/tắt linh hoạt
- 🔋 Giữ giờ chính xác khi mất điện (nhờ DS3231 có pin backup)
- 🚫 Không dùng `delay()` cho debounce (dùng `millis()`)

## Thiết bị
| Thiết bị | Số lượng |
|----------|:--------:|
| Arduino Uno R3 + cáp USB | 1 |
| LCD 16x2 I2C | 1 |
| RTC DS3231 | 1 |
| Nút nhấn | 3 |
| Piezo Buzzer | 1 |
| Breadboard + Jumper wires | 1 bộ |

## Cấu trúc dự án
```
digital-clock---IOT/
├── digital_clock/
│   └── digital_clock.ino    # Code chính (Arduino sketch)
├── docs/
│   ├── wiring_and_setup.md  # Sơ đồ kết nối & cài đặt
│   └── team_assignment.md   # Phân chia code & bảng test
└── README.md
```

## Bắt đầu nhanh

### 1. Cài thư viện (Arduino IDE)
- `Sketch` → `Include Library` → `Manage Libraries`
- Tìm & cài: **RTClib** (Adafruit) + **LiquidCrystal I2C** (Frank de Brabander)

### 2. Kết nối phần cứng
Xem chi tiết: [`docs/wiring_and_setup.md`](docs/wiring_and_setup.md)

### 3. Upload code
- Mở `digital_clock/digital_clock.ino` bằng Arduino IDE
- Chọn Board: **Arduino Uno**, Port: **COMx**
- Nhấn **Upload** (→)

### 4. Sử dụng
| Nút | Chức năng |
|-----|-----------|
| **MODE** (D2) | Chuyển chế độ: Xem → Chỉnh Giờ → Chỉnh Phút → Alarm Giờ → Alarm Phút → Xem |
| **UP** (D3) | Tăng giá trị / Bật-tắt báo thức |
| **DOWN** (D4) | Giảm giá trị / Bật-tắt báo thức |

## Tài liệu
- [Sơ đồ kết nối & Hướng dẫn cài đặt](docs/wiring_and_setup.md)
- [Phân chia code & Bảng kiểm thử](docs/team_assignment.md)