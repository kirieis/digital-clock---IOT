# Phân Chia Code Theo Thành Viên - Group 1

## Cấu trúc file: `digital_clock/digital_clock.ino`

Toàn bộ code nằm trong **1 file duy nhất**, chia thành 6 phần rõ ràng. Mỗi người chịu trách nhiệm **hiểu, giải thích, và debug** phần code của mình.

---

## Bảng Phân Công

| Người | Tên | Phần code | Dòng (ước tính) | Hàm chính |
|:---:|------|-----------|:---:|-----------|
| **1** | Trưởng nhóm | Phần 1 (Khai báo) + Phần 6 (`setup`/`loop`) | 40-90 & 465+ | `setup()`, `loop()` |
| **2** | Chuyên gia RTC | Phần 2 (Module RTC) | 95-145 | `initRTC()`, `readTime()`, `setTime()` |
| **3** | Chuyên gia LCD | Phần 3 (Module LCD) | 150-280 | `initLCD()`, `displayTime()`, `displayDate()`, `displaySetMode()`, `displayNormal()` |
| **4** | Chuyên gia Nút nhấn | Phần 4 (Nút nhấn & State Machine) | 285-385 | `initButtons()`, `isButtonPressed()`, `wrapValue()`, `handleButtons()` |
| **5** | Chuyên gia Báo thức & QA | Phần 5 (Báo thức) + Test | 390-460 | `initBuzzer()`, `checkAlarm()`, `triggerAlarm()`, `stopAlarm()` |

---

## Chi Tiết Từng Người

### 👤 Người 1 — Trưởng nhóm (Nguyễn Văn An / Trần Công Tiến / ...)
**Trách nhiệm:**
- Khai báo thư viện, define chân, biến toàn cục
- Viết `setup()` gọi tất cả hàm init
- Viết `loop()` điều phối luồng chương trình
- Ghép code, debug xung đột
- Vẽ sơ đồ kết nối cho cả nhóm

**Câu hỏi vấn đáp mẫu:**
> Q: Tại sao dùng `enum ClockMode` thay vì `int`?  
> A: Enum giúp code dễ đọc hơn, tránh nhầm giá trị số. Ví dụ `MODE_SET_HOUR` rõ ràng hơn `1`.

> Q: Tại sao `loop()` gọi `readTime()` trước `handleButtons()`?  
> A: Cần đọc giờ trước để nút UP/DOWN biết giá trị hiện tại khi vào chế độ chỉnh.

---

### 👤 Người 2 — Chuyên gia RTC
**Trách nhiệm:**
- Kết nối phần cứng DS3231
- Hiểu cách giao tiếp I2C
- Giải thích 3 hàm: `initRTC()`, `readTime()`, `setTime()`

**Câu hỏi vấn đáp mẫu:**
> Q: Chuyện gì xảy ra khi rút USB (mất nguồn)?  
> A: DS3231 có pin CR2032, giữ giờ chính xác. Khi cắm lại, `rtc.lostPower()` trả `false` nên giờ không bị reset.

> Q: `rtc.adjust(DateTime(F(__DATE__), F(__TIME__)))` nghĩa là gì?  
> A: Set giờ RTC theo thời gian máy tính lúc biên dịch code. Chỉ chạy 1 lần khi RTC mất pin.

---

### 👤 Người 3 — Chuyên gia LCD
**Trách nhiệm:**
- Kết nối phần cứng LCD I2C
- Hiểu cách hiển thị trên LCD 16x2
- Giải thích 5+ hàm hiển thị

**Câu hỏi vấn đáp mẫu:**
> Q: Tại sao cần hàm `printTwoDigits()`?  
> A: Để hiện "09:05" thay vì "9:5". Thêm số 0 phía trước nếu giá trị < 10.

> Q: `lcd.setCursor(0, 0)` và `lcd.setCursor(0, 1)` khác gì?  
> A: `(0,0)` = cột 0 dòng 1 (trên), `(0,1)` = cột 0 dòng 2 (dưới). LCD 16x2 có 2 dòng × 16 cột.

---

### 👤 Người 4 — Chuyên gia Nút nhấn
**Trách nhiệm:**
- Lắp 3 nút trên breadboard
- Hiểu debounce và state machine
- Giải thích logic `handleButtons()`

**Câu hỏi vấn đáp mẫu:**
> Q: Tại sao không dùng `delay()` để debounce?  
> A: `delay()` dừng TOÀN BỘ chương trình. Đồng hồ sẽ đứng yên trong lúc delay. Dùng `millis()` cho phép code tiếp tục chạy.

> Q: `INPUT_PULLUP` hoạt động thế nào?  
> A: Arduino kích hoạt điện trở kéo lên bên trong (~20kΩ). Khi không nhấn = HIGH, khi nhấn = LOW (nối GND).

---

### 👤 Người 5 — Chuyên gia Báo thức & QA
**Trách nhiệm:**
- Kết nối buzzer
- Hiểu logic alarm ON/OFF
- Viết & chạy test cases

**Câu hỏi vấn đáp mẫu:**
> Q: Tại sao `triggerAlarm()` không dùng `delay()`?  
> A: Tương tự debounce — cần giữ `loop()` chạy liên tục để nhấn nút tắt chuông được.

> Q: `tone(BUZZER, 1000)` nghĩa là gì?  
> A: Phát sóng vuông tần số 1000Hz qua chân BUZZER. Âm thanh giống tiếng bíp.

---

## 📋 Bảng Kiểm Thử (QA)

| # | Test Case | Cách test | Kết quả mong đợi | Pass/Fail |
|:---:|-----------|----------|-------------------|:---------:|
| 1 | Khởi động | Upload code, mở Serial Monitor | Hiện "All modules initialized" | ⬜ |
| 2 | Hiển thị giờ | Xem LCD | Dòng 1: giờ, Dòng 2: ngày/alarm xen kẽ | ⬜ |
| 3 | Mất điện | Rút USB → đợi 10s → cắm lại | Giờ vẫn chính xác | ⬜ |
| 4 | Chỉnh giờ | Nhấn MODE → UP/DOWN → MODE | Giờ thay đổi đúng | ⬜ |
| 5 | Wrap giờ | Chỉnh giờ từ 23 nhấn UP | Về 0 | ⬜ |
| 6 | Wrap phút | Chỉnh phút từ 0 nhấn DOWN | Về 59 | ⬜ |
| 7 | Đặt báo thức | MODE qua các chế độ alarm | Hiện "Alarm set: HH:MM" trên Serial | ⬜ |
| 8 | Báo thức kêu | Đặt alarm = giờ hiện tại + 1 phút | Buzzer kêu bíp bíp | ⬜ |
| 9 | Tắt báo thức | Nhấn nút khi chuông kêu | Chuông tắt, về màn hình chính | ⬜ |
| 10 | Bật/tắt alarm | Ở chế độ hiển thị, nhấn UP | Alarm toggle ON/OFF | ⬜ |
