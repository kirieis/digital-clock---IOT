# Smart Digital Clock with Temperature Monitoring (ESP8266) 🕐🌡️

Hệ thống Đồng Hồ Điện Tử Thông Minh giám sát Nhiệt Độ Môi Trường thời gian thực dựa trên bo mạch **ESP8266 (NodeMCU v2/v3 / Wemos D1 Mini)**, kết nối trực tiếp với Giao Diện Web Dashboard qua công nghệ **Web Serial API**.

---

## 🌟 Tính năng nổi bật

### 1. Bo mạch ESP8266 (NodeMCU)
- ⏰ **Hiển thị Thời Gian Thực**: Đồng bộ chính xác **Giờ : Phút : Giây** và **Ngày / Tháng / Năm** (Module DS3231 / DS1307 RTC).
- 🌡️ **Đo Nhiệt Độ (°C / °F)**: Đọc liên tục nhiệt độ môi trường từ cảm biến NTC Analog đấu nối trực tiếp vào **Pin A0**.
- 📡 **Truyền Dữ Liệu Thời Gian Thực**: Phát dữ liệu chuỗi JSON qua cổng Serial với tốc độ `115200 baud`.

### 2. Giao diện Web Dashboard (`digital_clock_simulator.html`)
- 🔌 **Kết Nối Trực Tiếp Phần Cứng (Web Serial API)**: Cắm cáp USB nối ESP8266 vào máy tính ➔ Bấm *"Kết Nối Hardware ESP8266"* trên trình duyệt Chrome/Edge là Web tự động nhận dữ liệu thời gian thực mà không cần phần mềm trung gian.
- 🖥️ **Hiển Thị Đồng Hồ Điện Tử Cỡ Lớn**: Đèn nền LCD retro sắc nét (Cyan, Emerald, Amber) với hiệu ứng scanline.
- 📊 **Đồ Thị Biến Thiên Real-time**: Theo dõi trực quan đồ thị sóng biến thiên Nhiệt độ qua thời gian.
- 🎛️ **Chế Độ Giả Lập Pin A0 (Simulation Mode)**: Tự động giả lập dữ liệu cảm biến A0 nếu chưa cắm phần cứng thật để test dễ dàng.

---

## 🔌 Sơ đồ kết nối phần cứng ESP8266 (NodeMCU)

```
        NodeMCU ESP8266                    Thiết Bị / Cảm Biến
┌───────────────────────────┐       ┌───────────────────────┐
│ Pin D2 (GPIO 4 - SDA) ────┼───────┤ LCD 16x2 I2C (SDA)    │
│ Pin D1 (GPIO 5 - SCL) ────┼───────┤ LCD 16x2 I2C (SCL)    │
│                           │       ├───────────────────────┤
│ Pin D2 (GPIO 4 - SDA) ────┼───────┤ RTC DS3231 (SDA)      │
│ Pin D1 (GPIO 5 - SCL) ────┼───────┤ RTC DS3231 (SCL)      │
│                           │       ├───────────────────────┤
│ Pin A0 (ADC0) ────────────┼───────┤ Cảm biến NTC Analog   │
│                           │       ├───────────────────────┤
│ 3.3V / 5V ────────────────┼───────┤ VCC Linh Kiện         │
│ GND ──────────────────────┼───────┤ GND Linh Kiện         │
└───────────────────────────┘       └───────────────────────┘
```

---

## 🚀 Hướng dẫn khởi động nhanh

### 1. Nạp code cho ESP8266
1. Mở file [digital_clock/digital_clock.ino](digital_clock/digital_clock.ino) bằng **Arduino IDE**.
2. Chọn Bo mạch: **NodeMCU 1.0 (ESP-12E Module)** (hoặc Generic ESP8266 Module).
3. Chọn đúng cổng **COMx** của bạn.
4. Nhấn **Upload** (➔).

### 2. Mở Giao diện Web Dashboard
- Nhấp đúp file `startup.bat` để bật server giả lập Web tại `http://localhost:6009/digital_clock_simulator.html`.
- Hoặc mở trực tiếp file `digital_clock_simulator.html` trên trình duyệt Google Chrome hoặc Microsoft Edge.

### 3. Kết nối phần cứng thật trên Web
1. Cắm cáp USB nối ESP8266 với máy tính.
2. Trên Web Dashboard, bấm nút **"Kết Nối Hardware ESP8266"** ở góc phải thanh Header.
3. Chọn cổng COM tương ứng của ESP8266 ➔ Nhấn **Connect**.
4. Màn hình Web sẽ lập tức nhận dữ liệu Giờ, Ngày, và Nhiệt độ thời gian thực!