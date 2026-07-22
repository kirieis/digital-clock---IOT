/*
 * ====================================================================
 *  SMART AI DESK COMPANION (ESP8266 / NodeMCU)
 * ====================================================================
 *  Bo mạch: ESP8266 (NodeMCU v2/v3 / Wemos D1 Mini)
 * 
 *  Tính năng nâng cấp:
 *    - Đọc & Đồng bộ thời gian thực: Giờ : Phút : Giây & Ngày / Tháng / Năm (DS3231/DS1307 RTC / Internal)
 *    - Cảm biến Nhiệt độ & Độ ẩm DHT11 (Chân D3 / GPIO0)
 *    - Cảm biến Chuyển động PIR HC-SR501 (Chân D2 / GPIO4)
 *    - Gửi chuỗi dữ liệu JSON qua Serial (Baudrate: 115200) theo thời gian thực tới Web Dashboard
 *    - Hiển thị trên màn hình LCD 16x2 I2C (0x27) (Tùy chọn nếu cắm LCD)
 *    - Nhận lệnh cấu hình từ Web Serial API
 * 
 *  Sơ đồ kết nối chân ESP8266 (NodeMCU):
 *    - Cảm biến DHT11 (Nhiệt/Độ ẩm): Pin D3 (GPIO0), VCC -> 3.3V/5V, GND -> GND
 *    - Cảm biến PIR HC-SR501 (Chuyển động): Pin D2 (GPIO4), VCC -> 5V, GND -> GND
 *    - LCD 1602 I2C / RTC DS3231 (Tùy chọn): SDA -> GPIO4 (D2) / SCL -> GPIO5 (D1)
 *    - Cảm biến Analog NTC (Fallback): Pin A0 (ADC0)
 * ====================================================================
 */

#include <ESP8266WiFi.h>
#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// --- Cấu hình chân ESP8266 ---
#define DHTPIN     0    // NodeMCU D3 (GPIO0) - Cảm biến DHT11
#define DHTTYPE    DHT11
#define PIR_PIN    4    // NodeMCU D2 (GPIO4) - Cảm biến chuyển động PIR HC-SR501
#define NTC_PIN    A0   // Chân đọc cảm biến Analog A0 (NTC / Fallback)

#define SDA_PIN    4    // NodeMCU D2 (GPIO4) - Bus I2C
#define SCL_PIN    5    // NodeMCU D1 (GPIO5) - Bus I2C

// --- Cấu hình LCD1602 I2C ---
#define LCD_ADDR   0x27
#define LCD_COLS   16
#define LCD_ROWS   2

// Khởi tạo các đối tượng phần cứng
DHT dht(DHTPIN, DHTTYPE);
RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// Biến trạng thái linh kiện
bool rtcFound = false;
bool lcdFound = false;

// Biến thời gian & môi trường
int hour   = 21;
int minute = 0;
int second = 0;
int day    = 22;
int month  = 7;
int year   = 2026;

float temperature = 28.2; // Nhiệt độ (°C)
float humidity    = 65.0; // Độ ẩm (%)
int   motionState = 0;    // 1: Có người ở bàn, 0: Vắng mặt

unsigned long lastSensorRead = 0;
unsigned long lastSerialSend = 0;
const unsigned long SENSOR_INTERVAL = 1500; // Đọc cảm biến mỗi 1.5s
const unsigned long SERIAL_INTERVAL = 1000; // Gửi dữ liệu Serial mỗi 1s

// ====================================================================
// CHƯƠNG TRÌNH KHỞI TẠO (SETUP)
// ====================================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println(F("[SYSTEM] Connecting ESP8266 Smart AI Desk Companion..."));

  // 1. Khởi tạo Cảm biến DHT11 & PIR
  dht.begin();
  pinMode(PIR_PIN, INPUT);
  pinMode(NTC_PIN, INPUT);

  // 2. Khởi tạo bus I2C (SDA=D2, SCL=D1) & LCD 1602 (Nếu có)
  Wire.begin(SDA_PIN, SCL_PIN); 
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(F("Smart AI Desk"));
  lcd.setCursor(0, 1);
  lcd.print(F("ESP8266 Ready"));
  delay(1000);

  // 3. Khởi tạo RTC DS3231 / DS1307
  if (rtc.begin()) {
    rtcFound = true;
    Serial.println(F("[RTC] RTC Initialized successfully."));
    if (rtc.lostPower()) {
      Serial.println(F("[RTC] RTC lost power, setting compile time..."));
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  } else {
    Serial.println(F("[RTC] Note: RTC module not detected, using system clock."));
  }

  Serial.println(F("[SYSTEM] ESP8266 Ready for Web Serial API & AI Agent."));
}

// ====================================================================
// VÒNG LẶP CHÍNH (LOOP)
// ====================================================================
void loop() {
  unsigned long currentMillis = millis();

  // --- 1. ĐỌC THỜI GIỜ TỪ RTC/SYSTEM ---
  if (rtcFound) {
    DateTime now = rtc.now();
    hour   = now.hour();
    minute = now.minute();
    second = now.second();
    day    = now.day();
    month  = now.month();
    year   = now.year();
  } else {
    // Internal timer fallback
    static unsigned long lastTick = 0;
    if (currentMillis - lastTick >= 1000) {
      lastTick = currentMillis;
      second++;
      if (second >= 60) {
        second = 0;
        minute++;
        if (minute >= 60) {
          minute = 0;
          hour = (hour + 1) % 24;
        }
      }
    }
  }

  // --- 2. ĐỌC DHT11 VÀ CẢM BIẾN CHUYỂN ĐỘNG PIR (PHÁT HIỆN ĐỔI TRẠNG THÁI TỨC THÌ) ---
  int currentPIR = digitalRead(PIR_PIN);
  if (currentPIR != motionState) {
    motionState = currentPIR;
    sendJsonTelemetry(); // Gửi JSON ngay lập tức khi người đứng dậy rời bàn / quay lại bàn
  }

  if (currentMillis - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = currentMillis;
    readSensors();
  }

  // --- 3. CẬP NHẬT HIỂN THỊ MÀN HÌNH LCD ---
  updateLCDDisplay();

  // --- 4. GỬI TELEMETRY JSON XUỐNG SERIAL (MỖI 1 GIÂY) ---
  if (currentMillis - lastSerialSend >= SERIAL_INTERVAL) {
    lastSerialSend = currentMillis;
    sendJsonTelemetry();
  }

  // --- 5. XỬ LÝ LỆNH SERIAL NHẬN TỪ WEBSITE ---
  handleIncomingSerial();
}

// ====================================================================
// CÁC HÀM XỬ LÝ PHỤ
// ====================================================================

// Đọc giá trị từ DHT11 và PIR HC-SR501
void readSensors() {
  // 1. Đọc Cảm biến Chuyển động PIR
  motionState = digitalRead(PIR_PIN);

  // 2. Đọc Cảm biến DHT11 (Nhiệt độ & Độ ẩm)
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (!isnan(h) && !isnan(t)) {
    humidity = h;
    temperature = t;
  } else {
    // Fallback sang Pin Analog A0 nếu chưa cắm DHT11
    int rawAnalog = analogRead(NTC_PIN);
    if (rawAnalog > 10) {
      float voltage = rawAnalog * (3.3 / 1024.0);
      temperature = 25.0 + (1.65 - voltage) * 12.0;
      if (temperature < 10.0) temperature = 24.5;
      if (temperature > 55.0) temperature = 35.0;
    }
  }
}

// Hiển thị thông tin lên màn hình LCD 16x2 (Nếu có)
void updateLCDDisplay() {
  char line1[17];
  char line2[17];

  snprintf(line1, sizeof(line1), "%02d:%02d:%02d  %02d/%02d", hour, minute, second, day, month);
  snprintf(line2, sizeof(line2), "T:%.1fC H:%.0f%% P:%d", temperature, humidity, motionState);

  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

// Gửi định dạng JSON qua Serial cho Web Serial API
void sendJsonTelemetry() {
  char dateStr[11];
  char timeStr[9];

  snprintf(dateStr, sizeof(dateStr), "%02d/%02d/%04d", day, month, year);
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", hour, minute, second);

  // In chuỗi JSON chuẩn
  Serial.print(F("{\"time\":\""));
  Serial.print(timeStr);
  Serial.print(F("\",\"date\":\""));
  Serial.print(dateStr);
  Serial.print(F("\",\"temp\":"));
  Serial.print(temperature, 1);
  Serial.print(F(",\"humidity\":"));
  Serial.print(humidity, 1);
  Serial.print(F(",\"motion\":"));
  Serial.print(motionState);
  Serial.println(F(",\"status\":\"OK\"}"));
}

// Nhận và phân tích lệnh Serial gửi từ Website
void handleIncomingSerial() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command.length() == 0) return;

    if (command == "GET_STATUS" || command == "READ_TEMP") {
      sendJsonTelemetry();
    } 
    else if (command == "PING") {
      Serial.println(F("{\"ack\":\"PONG\",\"device\":\"ESP8266_SmartDeskAI\"}"));
    } 
    else if (command.startsWith("SET_TIME:")) {
      String timeVal = command.substring(9);
      int firstColon = timeVal.indexOf(':');
      int secondColon = timeVal.lastIndexOf(':');
      if (firstColon > 0 && secondColon > firstColon) {
        int h = timeVal.substring(0, firstColon).toInt();
        int m = timeVal.substring(firstColon + 1, secondColon).toInt();
        int s = timeVal.substring(secondColon + 1).toInt();
        if (h >= 0 && h <= 23 && m >= 0 && m <= 59 && s >= 0 && s <= 59) {
          hour = h; minute = m; second = s;
          if (rtcFound) {
            rtc.adjust(DateTime(year, month, day, hour, minute, second));
          }
          Serial.println(F("{\"ack\":\"SET_TIME_OK\"}"));
        }
      }
    } 
    else if (command.startsWith("SET_DATE:")) {
      String dateVal = command.substring(9);
      int firstSlash = dateVal.indexOf('/');
      int secondSlash = dateVal.lastIndexOf('/');
      if (firstSlash > 0 && secondSlash > firstSlash) {
        int d = dateVal.substring(0, firstSlash).toInt();
        int mo = dateVal.substring(firstSlash + 1, secondSlash).toInt();
        int y = dateVal.substring(secondSlash + 1).toInt();
        if (d >= 1 && d <= 31 && mo >= 1 && mo <= 12 && y >= 2000) {
          day = d; month = mo; year = y;
          if (rtcFound) {
            rtc.adjust(DateTime(year, month, day, hour, minute, second));
          }
          Serial.println(F("{\"ack\":\"SET_DATE_OK\"}"));
        }
      }
    }
    else if (command == "RESET_ESP") {
      Serial.println(F("{\"ack\":\"RESETTING...\"}"));
      delay(100);
      ESP.restart();
    }
  }
}
