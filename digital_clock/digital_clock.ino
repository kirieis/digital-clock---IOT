/*
 * ============================================================
 *  DIGITAL CLOCK - GROUP 1
 * ============================================================
 *  Thiết bị:
 *    - Arduino Uno R3
 *    - LCD 16x2 I2C (0x27)
 *    - RTC DS3231
 *    - 3 Nút nhấn (MODE, UP, DOWN)
 *    - Piezo Buzzer
 *
 *  Sơ đồ chân:
 *    A4 (SDA) → DS3231 SDA + LCD SDA
 *    A5 (SCL) → DS3231 SCL + LCD SCL
 *    D2       → Nút MODE  (INPUT_PULLUP, chân kia → GND)
 *    D3       → Nút UP    (INPUT_PULLUP, chân kia → GND)
 *    D4       → Nút DOWN  (INPUT_PULLUP, chân kia → GND)
 *    D8       → Buzzer (+) (chân kia → GND)
 *
 *  Thư viện cần cài:
 *    - RTClib (by Adafruit)
 *    - LiquidCrystal_I2C (by Frank de Brabander)
 * ============================================================
 *
 *  CẤU TRÚC CODE THEO PHÂN CÔNG NHÓM:
 *  ──────────────────────────────────
 *  [Người 1] Phần 1: Khai báo & Cấu hình  (dòng ~40-90)
 *  [Người 2] Phần 2: Module RTC            (dòng ~95-145)
 *  [Người 3] Phần 3: Module LCD            (dòng ~150-280)
 *  [Người 4] Phần 4: Module Nút nhấn       (dòng ~285-385)
 *  [Người 5] Phần 5: Module Báo thức       (dòng ~390-460)
 *  [Người 1] Phần 6: setup() & loop()      (dòng ~465-cuối)
 *  ──────────────────────────────────
 */

// ============================================================
// PHẦN 1: KHAI BÁO THƯ VIỆN & BIẾN TOÀN CỤC
// [Người 1 - Trưởng nhóm: Kiến trúc & Tích hợp]
// ============================================================

#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>

// --- Cấu hình chân ---
#define BTN_MODE  2    // Nút chuyển chế độ
#define BTN_UP    3    // Nút tăng giá trị
#define BTN_DOWN  4    // Nút giảm giá trị
#define BUZZER    8    // Còi buzzer

// --- Cấu hình LCD ---
// Nếu LCD không hiện, thử đổi 0x27 thành 0x3F
#define LCD_ADDR  0x27
#define LCD_COLS  16
#define LCD_ROWS  2

// --- Các chế độ (State Machine) ---
enum ClockMode {
  MODE_DISPLAY,         // Hiển thị giờ bình thường
  MODE_SET_HOUR,        // Chỉnh giờ
  MODE_SET_MINUTE,      // Chỉnh phút
  MODE_SET_ALARM_HOUR,  // Chỉnh giờ báo thức
  MODE_SET_ALARM_MINUTE // Chỉnh phút báo thức
};

// --- Biến trạng thái ---
ClockMode currentMode = MODE_DISPLAY;

// Biến thời gian hiện tại (đọc từ RTC)
int currentHour   = 0;
int currentMinute = 0;
int currentSecond = 0;
int currentDay    = 1;
int currentMonth  = 1;
int currentYear   = 2026;

// Biến tạm khi chỉnh giờ
int tempHour   = 0;
int tempMinute = 0;

// Biến báo thức
int  alarmHour    = 7;   // Giờ báo thức mặc định
int  alarmMinute  = 0;   // Phút báo thức mặc định
bool alarmEnabled = true; // Báo thức bật/tắt
bool alarmRinging = false; // Đang kêu chuông?

// Biến hiệu ứng nhấp nháy
bool blinkState = false;
unsigned long lastBlinkTime = 0;
const unsigned long BLINK_INTERVAL = 500; // ms

// Khởi tạo đối tượng
RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);


// ============================================================
// PHẦN 2: MODULE RTC - ĐỌC/GHI THỜI GIAN
// [Người 2 - Chuyên gia Thời gian]
// ============================================================

/*
 * Khởi tạo module RTC DS3231.
 * Nếu RTC mất nguồn hoặc chưa set giờ, tự động set theo
 * thời gian biên dịch code.
 */
void initRTC() {
  if (!rtc.begin()) {
    // RTC không tìm thấy - hiện lỗi trên Serial
    Serial.println(F("ERROR: RTC DS3231 not found!"));
    Serial.println(F("Check wiring: SDA->A4, SCL->A5, VCC->5V, GND->GND"));
    while (1); // Dừng chương trình
  }

  // Nếu RTC bị mất nguồn, set lại giờ theo thời gian compile
  if (rtc.lostPower()) {
    Serial.println(F("RTC lost power, setting time from compile..."));
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  Serial.println(F("RTC DS3231 initialized OK."));
}

/*
 * Đọc thời gian hiện tại từ RTC vào các biến toàn cục.
 * Gọi hàm này trong loop() mỗi vòng lặp.
 */
void readTime() {
  DateTime now = rtc.now();
  currentHour   = now.hour();
  currentMinute = now.minute();
  currentSecond = now.second();
  currentDay    = now.day();
  currentMonth  = now.month();
  currentYear   = now.year();
}

/*
 * Cập nhật giờ mới vào RTC.
 * Gọi khi người dùng chỉnh xong giờ/phút.
 */
void setTime(int h, int m, int s) {
  rtc.adjust(DateTime(currentYear, currentMonth, currentDay, h, m, s));
  Serial.print(F("Time set to: "));
  Serial.print(h); Serial.print(F(":")); 
  Serial.print(m); Serial.print(F(":")); 
  Serial.println(s);
}


// ============================================================
// PHẦN 3: MODULE LCD - HIỂN THỊ
// [Người 3 - Chuyên gia Hiển thị]
// ============================================================

/*
 * Khởi tạo LCD, bật đèn nền, hiện thông báo khởi động.
 */
void initLCD() {
  lcd.init();
  lcd.backlight();
  
  // Màn hình chào
  lcd.setCursor(2, 0);
  lcd.print(F("Digital Clock"));
  lcd.setCursor(4, 1);
  lcd.print(F("Group 1"));
  delay(2000);
  lcd.clear();
}

/*
 * In số có 2 chữ số (thêm "0" phía trước nếu < 10).
 * Ví dụ: 9 → "09", 12 → "12"
 */
void printTwoDigits(int value) {
  if (value < 10) lcd.print('0');
  lcd.print(value);
}

/*
 * Hiển thị giờ ở dòng 1 của LCD.
 * Format: "Time: HH:MM:SS"
 */
void displayTime(int h, int m, int s) {
  lcd.setCursor(0, 0);
  lcd.print(F("Time: "));
  printTwoDigits(h);
  lcd.print(':');
  printTwoDigits(m);
  lcd.print(':');
  printTwoDigits(s);
  lcd.print(' '); // Xóa ký tự thừa
}

/*
 * Hiển thị ngày ở dòng 2 của LCD.
 * Format: "Date: DD/MM/YYYY"
 */
void displayDate(int d, int mo, int y) {
  lcd.setCursor(0, 1);
  lcd.print(F("Date: "));
  printTwoDigits(d);
  lcd.print('/');
  printTwoDigits(mo);
  lcd.print('/');
  lcd.print(y);
}

/*
 * Hiển thị trạng thái báo thức ở dòng 2.
 * Format: "Alarm:HH:MM  ON" hoặc "Alarm:HH:MM OFF"
 */
void displayAlarmStatus() {
  lcd.setCursor(0, 1);
  lcd.print(F("Alarm:"));
  printTwoDigits(alarmHour);
  lcd.print(':');
  printTwoDigits(alarmMinute);
  lcd.print(alarmEnabled ? F("  ON") : F(" OFF"));
  lcd.print(' '); // Xóa ký tự thừa
}

/*
 * Hiển thị chế độ chỉnh giờ.
 * Dòng 1: Tên chế độ
 * Dòng 2: Giá trị (nhấp nháy khi đang chỉnh)
 */
void displaySetMode(const char* label, int value, int maxVal) {
  lcd.setCursor(0, 0);
  lcd.print(F("Set "));
  lcd.print(label);
  // Xóa phần còn lại dòng 1
  for (int i = strlen(label) + 4; i < LCD_COLS; i++) {
    lcd.print(' ');
  }
  
  lcd.setCursor(0, 1);
  lcd.print(F("Value: "));
  
  // Hiệu ứng nhấp nháy
  if (blinkState) {
    printTwoDigits(value);
  } else {
    lcd.print(F("  ")); // Ẩn giá trị
  }
  
  lcd.print(F("  (0-"));
  if (maxVal < 10) lcd.print('0');
  lcd.print(maxVal);
  lcd.print(F(")  "));
}

/*
 * Hiển thị màn hình chính (chế độ xem giờ bình thường).
 * Dòng 1: Thời gian
 * Dòng 2: Xen kẽ Ngày và Trạng thái báo thức mỗi 3 giây
 */
void displayNormal() {
  displayTime(currentHour, currentMinute, currentSecond);
  
  // Xen kẽ hiển thị Date và Alarm mỗi 3 giây
  if ((millis() / 3000) % 2 == 0) {
    displayDate(currentDay, currentMonth, currentYear);
  } else {
    displayAlarmStatus();
  }
}


// ============================================================
// PHẦN 4: MODULE NÚT NHẤN - DEBOUNCE & STATE MACHINE
// [Người 4 - Chuyên gia Tương tác]
// ============================================================

// Biến debounce riêng cho từng nút
unsigned long lastDebounceMode = 0;
unsigned long lastDebounceUp   = 0;
unsigned long lastDebounceDown = 0;
const unsigned long DEBOUNCE_DELAY = 200; // ms

/*
 * Khởi tạo các chân nút nhấn với điện trở kéo lên nội bộ.
 * INPUT_PULLUP: Không cần gắn điện trở ngoài.
 * Nút nhấn = LOW, Không nhấn = HIGH.
 */
void initButtons() {
  pinMode(BTN_MODE, INPUT_PULLUP);
  pinMode(BTN_UP,   INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
}

/*
 * Đọc nút có chống rung (Debounce) bằng millis().
 * KHÔNG dùng delay() để tránh làm đứng đồng hồ.
 * Trả về true nếu nút được nhấn hợp lệ.
 */
bool isButtonPressed(int pin, unsigned long &lastDebounce) {
  if (digitalRead(pin) == LOW) {
    if (millis() - lastDebounce > DEBOUNCE_DELAY) {
      lastDebounce = millis();
      return true;
    }
  }
  return false;
}

/*
 * Hàm wrap-around: đảm bảo giá trị nằm trong khoảng [0, maxVal].
 * Ví dụ: wrapValue(24, 23) → 0, wrapValue(-1, 23) → 23
 */
int wrapValue(int value, int maxVal) {
  if (value > maxVal) return 0;
  if (value < 0) return maxVal;
  return value;
}

/*
 * Xử lý logic chính khi nhấn nút.
 * Quản lý State Machine và thay đổi giá trị.
 */
void handleButtons() {
  // --- NÚT MODE: Chuyển chế độ ---
  if (isButtonPressed(BTN_MODE, lastDebounceMode)) {
    
    // Nếu đang kêu chuông → tắt chuông, quay về màn hình chính
    if (alarmRinging) {
      stopAlarm();
      currentMode = MODE_DISPLAY;
      lcd.clear();
      return;
    }
    
    switch (currentMode) {
      case MODE_DISPLAY:
        // Vào chế độ chỉnh giờ
        currentMode = MODE_SET_HOUR;
        tempHour = currentHour;
        tempMinute = currentMinute;
        lcd.clear();
        Serial.println(F("Mode: SET HOUR"));
        break;
        
      case MODE_SET_HOUR:
        currentMode = MODE_SET_MINUTE;
        lcd.clear();
        Serial.println(F("Mode: SET MINUTE"));
        break;
        
      case MODE_SET_MINUTE:
        // Lưu giờ/phút đã chỉnh vào RTC
        setTime(tempHour, tempMinute, 0);
        currentMode = MODE_SET_ALARM_HOUR;
        lcd.clear();
        Serial.println(F("Mode: SET ALARM HOUR"));
        break;
        
      case MODE_SET_ALARM_HOUR:
        currentMode = MODE_SET_ALARM_MINUTE;
        lcd.clear();
        Serial.println(F("Mode: SET ALARM MINUTE"));
        break;
        
      case MODE_SET_ALARM_MINUTE:
        // Quay về hiển thị bình thường
        alarmEnabled = true;
        currentMode = MODE_DISPLAY;
        lcd.clear();
        Serial.print(F("Alarm set: "));
        Serial.print(alarmHour); Serial.print(F(":"));
        Serial.println(alarmMinute);
        Serial.println(F("Mode: DISPLAY"));
        break;
    }
  }
  
  // --- NÚT UP: Tăng giá trị ---
  if (isButtonPressed(BTN_UP, lastDebounceUp)) {
    
    // Nếu đang kêu chuông → tắt chuông
    if (alarmRinging) {
      stopAlarm();
      return;
    }
    
    switch (currentMode) {
      case MODE_DISPLAY:
        // Ở chế độ hiển thị, UP bật/tắt báo thức
        alarmEnabled = !alarmEnabled;
        Serial.print(F("Alarm: ")); 
        Serial.println(alarmEnabled ? F("ON") : F("OFF"));
        break;
      case MODE_SET_HOUR:
        tempHour = wrapValue(tempHour + 1, 23);
        break;
      case MODE_SET_MINUTE:
        tempMinute = wrapValue(tempMinute + 1, 59);
        break;
      case MODE_SET_ALARM_HOUR:
        alarmHour = wrapValue(alarmHour + 1, 23);
        break;
      case MODE_SET_ALARM_MINUTE:
        alarmMinute = wrapValue(alarmMinute + 1, 59);
        break;
    }
  }
  
  // --- NÚT DOWN: Giảm giá trị ---
  if (isButtonPressed(BTN_DOWN, lastDebounceDown)) {
    
    // Nếu đang kêu chuông → tắt chuông
    if (alarmRinging) {
      stopAlarm();
      return;
    }
    
    switch (currentMode) {
      case MODE_DISPLAY:
        // Ở chế độ hiển thị, DOWN cũng bật/tắt báo thức
        alarmEnabled = !alarmEnabled;
        Serial.print(F("Alarm: "));
        Serial.println(alarmEnabled ? F("ON") : F("OFF"));
        break;
      case MODE_SET_HOUR:
        tempHour = wrapValue(tempHour - 1, 23);
        break;
      case MODE_SET_MINUTE:
        tempMinute = wrapValue(tempMinute - 1, 59);
        break;
      case MODE_SET_ALARM_HOUR:
        alarmHour = wrapValue(alarmHour - 1, 23);
        break;
      case MODE_SET_ALARM_MINUTE:
        alarmMinute = wrapValue(alarmMinute - 1, 59);
        break;
    }
  }
}


// ============================================================
// PHẦN 5: MODULE BÁO THỨC
// [Người 5 - Chuyên gia Âm thanh & Kiểm thử]
// ============================================================

// Biến điều khiển buzzer
unsigned long lastBuzzTime = 0;
bool buzzState = false;
const unsigned long BUZZ_ON_TIME  = 200;  // ms kêu
const unsigned long BUZZ_OFF_TIME = 300;  // ms nghỉ

/*
 * Khởi tạo chân buzzer.
 */
void initBuzzer() {
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);
}

/*
 * Kiểm tra xem có đến giờ báo thức chưa.
 * So sánh giờ:phút hiện tại với giờ:phút báo thức.
 * Chỉ kích hoạt nếu báo thức đang bật và chưa kêu.
 */
void checkAlarm() {
  if (!alarmEnabled || alarmRinging) return;
  
  if (currentHour == alarmHour && currentMinute == alarmMinute && currentSecond == 0) {
    alarmRinging = true;
    Serial.println(F("*** ALARM RINGING! ***"));
  }
}

/*
 * Phát tiếng bíp bíp liên tục (không dùng delay()).
 * Dùng millis() để tạo nhịp ON/OFF cho buzzer.
 * Gọi hàm này trong loop() khi alarmRinging = true.
 */
void triggerAlarm() {
  if (!alarmRinging) return;
  
  unsigned long now = millis();
  
  if (buzzState) {
    // Đang kêu → kiểm tra hết thời gian kêu chưa
    if (now - lastBuzzTime >= BUZZ_ON_TIME) {
      noTone(BUZZER);
      buzzState = false;
      lastBuzzTime = now;
    }
  } else {
    // Đang nghỉ → kiểm tra hết thời gian nghỉ chưa
    if (now - lastBuzzTime >= BUZZ_OFF_TIME) {
      tone(BUZZER, 1000); // 1000Hz
      buzzState = true;
      lastBuzzTime = now;
    }
  }
  
  // Hiển thị cảnh báo trên LCD
  lcd.setCursor(0, 0);
  lcd.print(F("** ALARM! **    "));
  lcd.setCursor(0, 1);
  lcd.print(F("Press any button"));
}

/*
 * Tắt chuông báo thức.
 * Gọi khi người dùng nhấn bất kỳ nút nào.
 */
void stopAlarm() {
  alarmRinging = false;
  buzzState = false;
  noTone(BUZZER);
  digitalWrite(BUZZER, LOW);
  lcd.clear();
  Serial.println(F("Alarm stopped."));
}


// ============================================================
// PHẦN 6: SETUP & LOOP
// [Người 1 - Trưởng nhóm: Tích hợp]
// ============================================================

void setup() {
  // Khởi tạo Serial Monitor để debug
  Serial.begin(9600);
  Serial.println(F("=== Digital Clock - Group 1 ==="));
  Serial.println(F("Initializing..."));
  
  // Khởi tạo I2C
  Wire.begin();
  
  // Khởi tạo từng module
  initRTC();      // [Người 2]
  initLCD();      // [Người 3]
  initButtons();  // [Người 4]
  initBuzzer();   // [Người 5]
  
  Serial.println(F("All modules initialized. Clock is running."));
  Serial.println(F("Controls:"));
  Serial.println(F("  MODE (D2): Switch mode"));
  Serial.println(F("  UP   (D3): Increase / Toggle alarm"));
  Serial.println(F("  DOWN (D4): Decrease / Toggle alarm"));
}

void loop() {
  // 1. Đọc thời gian từ RTC [Người 2]
  readTime();
  
  // 2. Cập nhật hiệu ứng nhấp nháy
  if (millis() - lastBlinkTime >= BLINK_INTERVAL) {
    blinkState = !blinkState;
    lastBlinkTime = millis();
  }
  
  // 3. Xử lý nút nhấn [Người 4]
  handleButtons();
  
  // 4. Kiểm tra báo thức [Người 5]
  checkAlarm();
  
  // 5. Xử lý chuông nếu đang kêu [Người 5]
  if (alarmRinging) {
    triggerAlarm();
    return; // Không hiển thị gì khác khi chuông kêu
  }
  
  // 6. Hiển thị theo chế độ hiện tại [Người 3]
  switch (currentMode) {
    case MODE_DISPLAY:
      displayNormal();
      break;
    case MODE_SET_HOUR:
      displaySetMode("Hour", tempHour, 23);
      break;
    case MODE_SET_MINUTE:
      displaySetMode("Minute", tempMinute, 59);
      break;
    case MODE_SET_ALARM_HOUR:
      displaySetMode("Alarm Hr", alarmHour, 23);
      break;
    case MODE_SET_ALARM_MINUTE:
      displaySetMode("Alarm Min", alarmMinute, 59);
      break;
  }
  
  // Delay nhỏ để ổn định LCD (không ảnh hưởng debounce)
  delay(100);
}
