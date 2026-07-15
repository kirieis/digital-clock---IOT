/*
 * ============================================================
 *  DIGITAL CLOCK & VOICE IOT GAMING ROOM SETUP - GROUP 1
 * ============================================================
 *  Thiết bị:
 *    - Arduino Uno R3
 *    - LCD 16x2 I2C (0x27)
 *    - RTC DS3231
 *    - 3 Nút nhấn (MODE, UP, DOWN)
 *    - Piezo Buzzer
 *    - 4-Channel Relay Shield (D10, D11, D12, D13)
 *
 *  Sơ đồ chân:
 *    A4 (SDA) → DS3231 SDA + LCD SDA
 *    A5 (SCL) → DS3231 SCL + LCD SCL
 *    D2       → Nút MODE  (INPUT_PULLUP, chân kia → GND)
 *    D3       → Nút UP    (INPUT_PULLUP, chân kia → GND)
 *    D4       → Nút DOWN  (INPUT_PULLUP, chân kia → GND)
 *    D8       → Buzzer (+) (chân kia → GND)
 *    D10      → Đèn LED RGB Gaming (Relay 1 / Đèn LED Strip)
 *    D11      → Quạt Tản Nhiệt PC (Relay 2 / Turbo Fan)
 *    D12      → Khóa Cửa Phòng Ngủ (Relay 3 / Smart Lock)
 *    D13      → Đèn Ngủ Cozy Lamp (Relay 4 / Cozy Lamp)
 * ============================================================
 */

#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

// --- Cấu hình chân ---
#define BTN_MODE  2    // Nút chuyển chế độ
#define BTN_UP    3    // Nút tăng giá trị
#define BTN_DOWN  4    // Nút giảm giá trị
#define BUZZER    8    // Còi buzzer D8

// --- Chân thiết bị Gaming Room ---
#define DEV_LED   10   // Dây LED RGB
#define DEV_FAN   11   // Quạt PC Turbo
#define DEV_LOCK  12   // Khóa cửa phòng
#define DEV_LAMP  13   // Đèn ngủ Cozy

// --- Cấu hình LCD ---
#define LCD_ADDR  0x27
#define LCD_COLS  16
#define LCD_ROWS  2

// --- Cấu trúc Luật hẹn giờ (Rules) ---
struct AutomationRule {
  bool active;
  byte hour;
  byte minute;
  byte pin;      // 10, 11, 12, hoặc 13
  byte action;   // 1 = Bật (ON), 0 = Tắt (OFF), 2 = Tạm dừng (Disabled)
};
#define NUM_RULES 4
AutomationRule rules[NUM_RULES];

// --- Các chế độ (State Machine) ---
enum ClockMode {
  MODE_DISPLAY,           // Hiển thị bình thường
  MODE_SET_HOUR,          // Chỉnh giờ hệ thống
  MODE_SET_MINUTE,        // Chỉnh phút hệ thống
  MODE_SET_ALARM_HOUR,    // Chỉnh giờ báo thức
  MODE_SET_ALARM_MINUTE,  // Chỉnh phút báo thức
  MODE_RULE_SELECT,       // Chọn Rule (1-4)
  MODE_RULE_HOUR,         // Chỉnh giờ của Rule
  MODE_RULE_MIN,          // Chỉnh phút của Rule
  MODE_RULE_DEV,          // Chọn chân thiết bị (D10-D13)
  MODE_RULE_ACT           // Chọn hành động (ON/OFF/DIS)
};

ClockMode currentMode = MODE_DISPLAY;
int tempRuleIdx = 0; // Rule đang được chỉnh sửa (0-3)

// Biến thời gian hiện tại
int currentHour   = 0;
int currentMinute = 0;
int currentSecond = 0;
int currentDay    = 1;
int currentMonth  = 1;
int currentYear   = 2026;

// Biến tạm khi chỉnh giờ
int tempHour   = 0;
int tempMinute = 0;

// Biến báo thức đơn giản
int  alarmHour    = 7;
int  alarmMinute  = 0;
bool alarmEnabled = true;
bool alarmRinging = false;

// Biến hiệu ứng nhấp nháy LCD
bool blinkState = false;
unsigned long lastBlinkTime = 0;
const unsigned long BLINK_INTERVAL = 500;

// --- Rickroll Music Variables & Notes ---
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_CS5 554
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_FS5 740
#define NOTE_G5  784
#define NOTE_A5  880
#define NOTE_B5  988

// Giai điệu điệp khúc Never Gonna Give You Up
int rickrollMelody[] = {
  NOTE_A4, NOTE_B4, NOTE_D5, NOTE_B4, NOTE_FS5, NOTE_FS5, NOTE_E5,
  NOTE_A4, NOTE_B4, NOTE_D5, NOTE_B4, NOTE_E5, NOTE_E5, NOTE_D5, NOTE_CS5, NOTE_B4,
  NOTE_A4, NOTE_B4, NOTE_D5, NOTE_B4, NOTE_D5, NOTE_E5, NOTE_CS5, NOTE_B4, NOTE_A4,
  NOTE_A4, NOTE_E5, NOTE_D5
};
int rickrollNoteDurations[] = {
  8, 8, 8, 8, 4, 4, 2,
  8, 8, 8, 8, 4, 4, 8, 8, 2,
  8, 8, 8, 8, 4, 8, 8, 8, 4,
  4, 4, 2
};

bool rickrollPlaying = false;
int currentRickrollNote = 0;
unsigned long noteStartTime = 0;
unsigned long noteDurationMs = 0;

RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// ============================================================
// PHẦN 2: MODULE EEPROM & LƯU TRỮ RULES
// ============================================================

void saveRules() {
  EEPROM.write(0, 0xA5); // Ghi magic byte để đánh dấu đã init
  EEPROM.put(1, rules);
  Serial.println(F("[EEPROM] Saved 4 rules successfully."));
}

void loadRules() {
  byte magic = EEPROM.read(0);
  if (magic == 0xA5) {
    EEPROM.get(1, rules);
    Serial.println(F("[EEPROM] Loaded rules successfully."));
  } else {
    // Nạp giá trị mặc định nếu EEPROM trống
    rules[0] = { true, 7, 0, DEV_LED, 1 };    // R1: Bật LED lúc 07:00
    rules[1] = { true, 8, 30, DEV_LED, 0 };   // R2: Tắt LED lúc 08:30
    rules[2] = { true, 18, 0, DEV_FAN, 1 };   // R3: Bật Quạt lúc 18:00
    rules[3] = { true, 22, 0, DEV_FAN, 0 };   // R4: Tắt Quạt lúc 22:00
    saveRules();
    Serial.println(F("[EEPROM] Initialized with default rules."));
  }
}

// ============================================================
// PHẦN 3: MODULE RTC - ĐỌC/GHI THỜI GIAN
// ============================================================

void initRTC() {
  if (!rtc.begin()) {
    Serial.println(F("ERROR: RTC DS3231 not found!"));
    while (1);
  }
  if (rtc.lostPower()) {
    Serial.println(F("RTC lost power, setting time from compile..."));
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  Serial.println(F("RTC DS3231 initialized OK."));
}

void readTime() {
  DateTime now = rtc.now();
  currentHour   = now.hour();
  currentMinute = now.minute();
  currentSecond = now.second();
  currentDay    = now.day();
  currentMonth  = now.month();
  currentYear   = now.year();
}

void setTime(int h, int m, int s) {
  rtc.adjust(DateTime(currentYear, currentMonth, currentDay, h, m, s));
  Serial.print(F("Time set to: "));
  Serial.print(h); Serial.print(F(":")); 
  Serial.print(m); Serial.print(F(":")); 
  Serial.println(s);
}

// ============================================================
// PHẦN 4: MODULE LCD - HIỂN THỊ
// ============================================================

void initLCD() {
  lcd.init();
  lcd.backlight();
  lcd.setCursor(1, 0);
  lcd.print(F("Voice Gaming"));
  lcd.setCursor(4, 1);
  lcd.print(F("IoT Hub R1"));
  delay(1500);
  lcd.clear();
}

void printTwoDigits(int value) {
  if (value < 10) lcd.print('0');
  lcd.print(value);
}

void displayTime(int h, int m, int s) {
  lcd.setCursor(0, 0);
  lcd.print(F("TIME: "));
  printTwoDigits(h);
  lcd.print(':');
  printTwoDigits(m);
  lcd.print(':');
  printTwoDigits(s);
  lcd.print(' ');
}

void displayDate(int d, int mo, int y) {
  lcd.setCursor(0, 1);
  lcd.print(F("DATE: "));
  printTwoDigits(d);
  lcd.print('/');
  printTwoDigits(mo);
  lcd.print('/');
  lcd.print(y);
}

void displayAlarmStatus() {
  lcd.setCursor(0, 1);
  lcd.print(F("ALARM:"));
  printTwoDigits(alarmHour);
  lcd.print(':');
  printTwoDigits(alarmMinute);
  lcd.print(alarmEnabled ? F("  ON") : F(" OFF"));
  lcd.print(' ');
}

void displaySetMode(const char* label, int value, int maxVal) {
  lcd.setCursor(0, 0);
  lcd.print(F("Set "));
  lcd.print(label);
  for (int i = strlen(label) + 4; i < LCD_COLS; i++) {
    lcd.print(' ');
  }
  
  lcd.setCursor(0, 1);
  lcd.print(F("Value: "));
  if (blinkState) {
    printTwoDigits(value);
  } else {
    lcd.print(F("  "));
  }
  lcd.print(F("  (0-"));
  if (maxVal < 10) lcd.print('0');
  lcd.print(maxVal);
  lcd.print(F(") "));
}

// Hàm hiển thị cấu hình Rule chuyên dụng trên LCD 16x2
void displayRuleSetup(const char* label, int value, int step) {
  lcd.setCursor(0, 0);
  lcd.print(F("Setup R"));
  lcd.print(tempRuleIdx + 1);
  lcd.print(F(":"));
  lcd.print(label);
  int len = 8 + strlen(label);
  for (int i = len; i < LCD_COLS; i++) lcd.print(' ');
  
  lcd.setCursor(0, 1);
  if (step == 0) { // Select Rule
    lcd.print(F("Select Rule: R"));
    if (blinkState) lcd.print(value);
    else lcd.print(' ');
    lcd.print(F("  "));
  } else {
    lcd.print(F("T:"));
    
    // Giờ
    if (step == 1 && !blinkState) lcd.print(F("  "));
    else printTwoDigits(rules[tempRuleIdx].hour);
    lcd.print(':');
    
    // Phút
    if (step == 2 && !blinkState) lcd.print(F("  "));
    else printTwoDigits(rules[tempRuleIdx].minute);
    
    // Chân thiết bị
    lcd.print(F(" "));
    if (step == 3 && !blinkState) {
      lcd.print(F(" "));
    } else {
      byte p = rules[tempRuleIdx].pin;
      if (p == DEV_LED) lcd.print('D');
      else if (p == DEV_FAN) lcd.print('Q');
      else if (p == DEV_LOCK) lcd.print('K');
      else if (p == DEV_LAMP) lcd.print('N');
    }
    
    // Trạng thái: ON, OFF, DIS
    lcd.print(F(" "));
    if (step == 4 && !blinkState) {
      lcd.print(F("   "));
    } else {
      if (rules[tempRuleIdx].action == 1) lcd.print(F("ON "));
      else if (rules[tempRuleIdx].action == 0) lcd.print(F("OFF"));
      else lcd.print(F("DIS"));
    }
  }
}

void displayNormal() {
  displayTime(currentHour, currentMinute, currentSecond);
  
  // Xen kẽ Ngày -> Trạng thái Báo thức -> Trạng thái Rơ-le D10-D13 mỗi 3s
  int index = (millis() / 3000) % 3;
  if (index == 0) {
    displayDate(currentDay, currentMonth, currentYear);
  } else if (index == 1) {
    displayAlarmStatus();
  } else {
    // Trạng thái các rơ-le Gaming Room
    // L: LED (D10), F: FAN (D11), K: LOCK (D12), M: LAMP (D13)
    lcd.setCursor(0, 1);
    lcd.print(F("D:"));
    lcd.print(digitalRead(DEV_LED) ? 'B' : 'T');
    lcd.print(F(" Q:"));
    lcd.print(digitalRead(DEV_FAN) ? 'B' : 'T');
    lcd.print(F(" K:"));
    lcd.print(digitalRead(DEV_LOCK) ? 'K' : 'M');
    lcd.print(F(" N:"));
    lcd.print(digitalRead(DEV_LAMP) ? 'B' : 'T');
    lcd.print(F("  "));
  }
}

// ============================================================
// PHẦN 5: MODULE NÚT NHẤN & STATE MACHINE
// ============================================================

unsigned long lastDebounceMode = 0;
unsigned long lastDebounceUp   = 0;
unsigned long lastDebounceDown = 0;
const unsigned long DEBOUNCE_DELAY = 200;

void initButtons() {
  pinMode(BTN_MODE, INPUT_PULLUP);
  pinMode(BTN_UP,   INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
}

bool isButtonPressed(int pin, unsigned long &lastDebounce) {
  if (digitalRead(pin) == LOW) {
    if (millis() - lastDebounce > DEBOUNCE_DELAY) {
      lastDebounce = millis();
      return true;
    }
  }
  return false;
}

int wrapValue(int value, int maxVal) {
  if (value > maxVal) return 0;
  if (value < 0) return maxVal;
  return value;
}

// Helpers chuyển pin D10 -> D11 -> D12 -> D13 xoay vòng
int getNextPin(int currentPin) {
  if (currentPin == 10) return 11;
  if (currentPin == 11) return 12;
  if (currentPin == 12) return 13;
  return 10;
}
int getPrevPin(int currentPin) {
  if (currentPin == 10) return 13;
  if (currentPin == 11) return 10;
  if (currentPin == 12) return 11;
  return 12;
}

void handleButtons() {
  // Bấm bất cứ nút nào để tắt chuông báo thức hoặc tắt nhạc Rickroll
  if (isButtonPressed(BTN_MODE, lastDebounceMode) || 
      isButtonPressed(BTN_UP, lastDebounceUp) || 
      isButtonPressed(BTN_DOWN, lastDebounceDown)) {
    
    bool stopAction = false;
    
    if (alarmRinging) {
      alarmRinging = false;
      noTone(BUZZER);
      digitalWrite(BUZZER, LOW);
      lcd.clear();
      Serial.println(F("Alarm silenced by button press."));
      stopAction = true;
    }
    
    if (rickrollPlaying) {
      rickrollPlaying = false;
      noTone(BUZZER);
      digitalWrite(BUZZER, LOW);
      Serial.println(F("Rickroll silenced by button press."));
      stopAction = true;
    }
    
    if (stopAction) return;
    
    // Nếu không tắt chuông, thực thi chuyển chế độ (chỉ nút MODE)
    // Cần kiểm tra lại xem nút nào đã được nhấn
    if (digitalRead(BTN_MODE) == LOW) {
      switch (currentMode) {
        case MODE_DISPLAY:
          currentMode = MODE_SET_HOUR;
          tempHour = currentHour;
          tempMinute = currentMinute;
          lcd.clear();
          Serial.println(F("Mode: SET SYSTEM HOUR"));
          break;
        case MODE_SET_HOUR:
          currentMode = MODE_SET_MINUTE;
          lcd.clear();
          Serial.println(F("Mode: SET SYSTEM MINUTE"));
          break;
        case MODE_SET_MINUTE:
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
          alarmEnabled = true;
          currentMode = MODE_RULE_SELECT;
          tempRuleIdx = 0;
          lcd.clear();
          Serial.println(F("Mode: RULE SELECT"));
          break;
        case MODE_RULE_SELECT:
          currentMode = MODE_RULE_HOUR;
          lcd.clear();
          Serial.println(F("Mode: RULE HOUR"));
          break;
        case MODE_RULE_HOUR:
          currentMode = MODE_RULE_MIN;
          lcd.clear();
          Serial.println(F("Mode: RULE MINUTE"));
          break;
        case MODE_RULE_MIN:
          currentMode = MODE_RULE_DEV;
          lcd.clear();
          Serial.println(F("Mode: RULE DEVICE"));
          break;
        case MODE_RULE_DEV:
          currentMode = MODE_RULE_ACT;
          lcd.clear();
          Serial.println(F("Mode: RULE ACTION"));
          break;
        case MODE_RULE_ACT:
          saveRules();
          currentMode = MODE_DISPLAY;
          lcd.clear();
          Serial.println(F("Mode: DISPLAY (Saved to EEPROM)"));
          break;
      }
    }
  }

  // --- NÚT UP ---
  if (isButtonPressed(BTN_UP, lastDebounceUp)) {
    switch (currentMode) {
      case MODE_DISPLAY:
        alarmEnabled = !alarmEnabled;
        Serial.println(alarmEnabled ? F("Alarm: ON") : F("Alarm: OFF"));
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
      case MODE_RULE_SELECT:
        tempRuleIdx = wrapValue(tempRuleIdx + 1, 3);
        break;
      case MODE_RULE_HOUR:
        rules[tempRuleIdx].hour = wrapValue(rules[tempRuleIdx].hour + 1, 23);
        break;
      case MODE_RULE_MIN:
        rules[tempRuleIdx].minute = wrapValue(rules[tempRuleIdx].minute + 1, 59);
        break;
      case MODE_RULE_DEV:
        rules[tempRuleIdx].pin = getNextPin(rules[tempRuleIdx].pin);
        break;
      case MODE_RULE_ACT:
        rules[tempRuleIdx].action = (rules[tempRuleIdx].action == 1) ? 0 : ((rules[tempRuleIdx].action == 0) ? 2 : 1);
        rules[tempRuleIdx].active = (rules[tempRuleIdx].action != 2);
        break;
    }
  }

  // --- NÚT DOWN ---
  if (isButtonPressed(BTN_DOWN, lastDebounceDown)) {
    switch (currentMode) {
      case MODE_DISPLAY:
        alarmEnabled = !alarmEnabled;
        Serial.println(alarmEnabled ? F("Alarm: ON") : F("Alarm: OFF"));
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
      case MODE_RULE_SELECT:
        tempRuleIdx = wrapValue(tempRuleIdx - 1, 3);
        break;
      case MODE_RULE_HOUR:
        rules[tempRuleIdx].hour = wrapValue(rules[tempRuleIdx].hour - 1, 23);
        break;
      case MODE_RULE_MIN:
        rules[tempRuleIdx].minute = wrapValue(rules[tempRuleIdx].minute - 1, 59);
        break;
      case MODE_RULE_DEV:
        rules[tempRuleIdx].pin = getPrevPin(rules[tempRuleIdx].pin);
        break;
      case MODE_RULE_ACT:
        rules[tempRuleIdx].action = (rules[tempRuleIdx].action == 1) ? 2 : ((rules[tempRuleIdx].action == 0) ? 1 : 0);
        rules[tempRuleIdx].active = (rules[tempRuleIdx].action != 2);
        break;
    }
  }
}

// ============================================================
// PHẦN 6: BÁO THỨC & NHẠC RICKROLL KHÔNG CHẶN (NON-BLOCKING)
// ============================================================

unsigned long lastBuzzTime = 0;
bool buzzState = false;
const unsigned long BUZZ_ON_TIME  = 200;
const unsigned long BUZZ_OFF_TIME = 300;

void initBuzzer() {
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);
}

void checkAlarm() {
  if (!alarmEnabled || alarmRinging) return;
  if (currentHour == alarmHour && currentMinute == alarmMinute && currentSecond == 0) {
    alarmRinging = true;
    Serial.println(F("*** ALARM RINGING! ***"));
  }
}

void triggerAlarm() {
  if (!alarmRinging) return;
  
  unsigned long now = millis();
  if (buzzState) {
    if (now - lastBuzzTime >= BUZZ_ON_TIME) {
      noTone(BUZZER);
      buzzState = false;
      lastBuzzTime = now;
    }
  } else {
    if (now - lastBuzzTime >= BUZZ_OFF_TIME) {
      tone(BUZZER, 1000);
      buzzState = true;
      lastBuzzTime = now;
    }
  }
  
  lcd.setCursor(0, 0);
  lcd.print(F("** ALARM! **    "));
  lcd.setCursor(0, 1);
  lcd.print(F("Press any button"));
}

void playRickroll() {
  if (!rickrollPlaying) return;
  
  unsigned long now = millis();
  if (now - noteStartTime >= noteDurationMs) {
    int totalNotes = sizeof(rickrollMelody) / sizeof(rickrollMelody[0]);
    if (currentRickrollNote >= totalNotes) {
      rickrollPlaying = false;
      noTone(BUZZER);
      digitalWrite(BUZZER, LOW);
      Serial.println(F("[BUZZER] Rickroll playback finished."));
      return;
    }
    
    int noteFreq = rickrollMelody[currentRickrollNote];
    int noteDur = rickrollNoteDurations[currentRickrollNote];
    
    noteDurationMs = 1200 / noteDur;
    
    if (noteFreq > 0) {
      tone(BUZZER, noteFreq, noteDurationMs * 0.9);
    } else {
      noTone(BUZZER);
    }
    
    if (currentRickrollNote == 0) {
      Serial.println(F("[BUZZER] 🎵 Playing: Never Gonna Give You Up..."));
    }
    
    currentRickrollNote++;
    noteStartTime = now;
  }
}

void checkAutomationRules() {
  if (currentSecond == 0) {
    for (int i = 0; i < NUM_RULES; i++) {
      if (rules[i].active && rules[i].hour == currentHour && rules[i].minute == currentMinute) {
        bool state = (rules[i].action == 1);
        digitalWrite(rules[i].pin, state);
        
        Serial.print(F("[SCHEDULER] Triggered Rule "));
        Serial.print(i + 1);
        Serial.print(F(": Pin D"));
        Serial.print(rules[i].pin);
        Serial.println(state ? F(" -> ON") : F(" -> OFF"));
      }
    }
  }
}

// ============================================================
// PHẦN 7: BỘ ĐỌC SERIAL COMMAND PARSER
// ============================================================

void checkSerialCommand() {
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd.startsWith("CMD:")) {
      String sub = cmd.substring(4);
      int colonIdx = sub.indexOf(':');
      if (colonIdx != -1) {
        String device = sub.substring(0, colonIdx);
        String val = sub.substring(colonIdx + 1);
        
        if (device == "LED") {
          if (val == "1" || val == "R" || val == "G" || val == "B" || val == "P" || val == "W") {
            digitalWrite(DEV_LED, HIGH);
            Serial.println("ACK:LED:" + val);
          } else {
            digitalWrite(DEV_LED, LOW);
            Serial.println(F("ACK:LED:0"));
          }
        }
        else if (device == "FAN") {
          if (val == "1") {
            digitalWrite(DEV_FAN, HIGH);
            Serial.println(F("ACK:FAN:1"));
          } else {
            digitalWrite(DEV_FAN, LOW);
            Serial.println(F("ACK:FAN:0"));
          }
        }
        else if (device == "LCK") {
          if (val == "1") {
            digitalWrite(DEV_LOCK, HIGH);
            Serial.println(F("ACK:LCK:1"));
          } else {
            digitalWrite(DEV_LOCK, LOW);
            Serial.println(F("ACK:LCK:0"));
          }
        }
        else if (device == "LMP") {
          if (val == "1") {
            digitalWrite(DEV_LAMP, HIGH);
            Serial.println(F("ACK:LMP:1"));
          } else {
            digitalWrite(DEV_LAMP, LOW);
            Serial.println(F("ACK:LMP:0"));
          }
        }
        else if (device == "BZZ") {
          if (val == "R") {
            rickrollPlaying = true;
            currentRickrollNote = 0;
            noteStartTime = millis();
            noteDurationMs = 0;
            Serial.println(F("ACK:BZZ:R"));
          } else if (val == "0") {
            rickrollPlaying = false;
            noTone(BUZZER);
            digitalWrite(BUZZER, LOW);
            Serial.println(F("ACK:BZZ:0"));
          }
        }
        else if (device == "ALM") {
          // Format val: HH:MM
          int colonIdx2 = val.indexOf(':');
          if (colonIdx2 != -1) {
            int hr = val.substring(0, colonIdx2).toInt();
            int mn = val.substring(colonIdx2 + 1).toInt();
            if (hr >= 0 && hr <= 23 && mn >= 0 && mn <= 59) {
              alarmHour = hr;
              alarmMinute = mn;
              alarmEnabled = true;
              Serial.println("ACK:ALM:" + String(alarmHour) + ":" + String(alarmMinute));
            }
          }
        }
        else if (device == "RUL") {
          // Format val: Idx:HH:MM:Pin:Action
          int firstColon = val.indexOf(':');
          if (firstColon != -1) {
            int idx = val.substring(0, firstColon).toInt() - 1; // 1-indexed to 0-indexed
            if (idx >= 0 && idx < NUM_RULES) {
              String subVal = val.substring(firstColon + 1);
              int secondColon = subVal.indexOf(':');
              if (secondColon != -1) {
                int hr = subVal.substring(0, secondColon).toInt();
                String subVal2 = subVal.substring(secondColon + 1);
                int thirdColon = subVal2.indexOf(':');
                if (thirdColon != -1) {
                  int mn = subVal2.substring(0, thirdColon).toInt();
                  String subVal3 = subVal2.substring(thirdColon + 1);
                  int fourthColon = subVal3.indexOf(':');
                  if (fourthColon != -1) {
                    int pin = subVal3.substring(0, fourthColon).toInt();
                    int act = subVal3.substring(fourthColon + 1).toInt();
                    
                    if (hr >= 0 && hr <= 23 && mn >= 0 && mn <= 59 && (pin >= 10 && pin <= 13) && (act >= 0 && act <= 2)) {
                      rules[idx].hour = hr;
                      rules[idx].minute = mn;
                      rules[idx].pin = pin;
                      rules[idx].action = act;
                      rules[idx].active = (act != 2);
                      saveRules();
                      Serial.println("ACK:RUL:" + String(idx + 1) + ":" + String(hr) + ":" + String(mn) + ":" + String(pin) + ":" + String(act));
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

// ============================================================
// PHẦN 8: SETUP & LOOP
// ============================================================

void setup() {
  Serial.begin(9600);
  Serial.println(F("=== IoT Gaming Room Hub ==="));
  Serial.println(F("Initializing system..."));
  
  Wire.begin();
  
  pinMode(DEV_LED,  OUTPUT);
  pinMode(DEV_FAN,  OUTPUT);
  pinMode(DEV_LOCK, OUTPUT);
  pinMode(DEV_LAMP, OUTPUT);
  digitalWrite(DEV_LED,  LOW);
  digitalWrite(DEV_FAN,  LOW);
  digitalWrite(DEV_LOCK, HIGH); // Mặc định khóa cửa đóng (HIGH = LOCK)
  digitalWrite(DEV_LAMP, LOW);
  
  initRTC();
  initLCD();
  initButtons();
  initBuzzer();
  loadRules();
  
  Serial.println(F("System operational."));
}

void loop() {
  readTime();
  
  if (millis() - lastBlinkTime >= BLINK_INTERVAL) {
    blinkState = !blinkState;
    lastBlinkTime = millis();
  }
  
  handleButtons();
  checkSerialCommand();
  checkAutomationRules();
  checkAlarm();
  playRickroll();
  
  if (alarmRinging) {
    triggerAlarm();
    return;
  }
  
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
    case MODE_RULE_SELECT:
      displayRuleSetup("Rule Select", tempRuleIdx + 1, 0);
      break;
    case MODE_RULE_HOUR:
      displayRuleSetup("Rule Hour", rules[tempRuleIdx].hour, 1);
      break;
    case MODE_RULE_MIN:
      displayRuleSetup("Rule Min", rules[tempRuleIdx].minute, 2);
      break;
    case MODE_RULE_DEV:
      displayRuleSetup("Rule Dev", rules[tempRuleIdx].pin, 3);
      break;
    case MODE_RULE_ACT:
      displayRuleSetup("Rule Action", rules[tempRuleIdx].action, 4);
      break;
  }
  
  delay(100);
}
