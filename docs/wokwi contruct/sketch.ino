/*
  ====================================================================
  IoT Digital Clock — ESP8266 (Tương thích mọi bo mạch ESP8266)
  --------------------------------------------------------------------
  Sơ đồ chân ESP8266:
    - LCD1602 I2C (0x27)  — SDA: GPIO4 (NodeMCU D2), SCL: GPIO5 (NodeMCU D1)
    - DS1307 RTC (I2C)    — SDA: GPIO4 (NodeMCU D2), SCL: GPIO5 (NodeMCU D1)
    - NTC Sensor (Analog) — VCC (3.3V), GND, OUT -> Pin A0
  ====================================================================
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>

// Định nghĩa chân I2C tương thích cả NodeMCU lẫn Generic ESP8266
#ifndef D2
  #define D2 4  // GPIO4 (SDA)
#endif
#ifndef D1
  #define D1 5  // GPIO5 (SCL)
#endif

// ------------------- WiFi Config -------------------
const char* ssid     = "Wokwi-GUEST"; // Thay tên WiFi của bạn
const char* password = "";             // Thay mật khẩu WiFi

// ------------------- ThingsBoard / MQTT Config -------------------
const char* mqtt_server  = "demo.thingsboard.io";
const int   mqtt_port    = 1883;
const char* device_token = "exzBQwFLEN6mbWHMCGh9";
const char* mqtt_topic   = "v1/devices/me/telemetry";

WiFiClient   espClient;
PubSubClient client(espClient);

// ------------------- NTP Config -------------------
const char* ntpServer          = "pool.ntp.org";
const long  gmtOffset_sec      = 7 * 3600;   // GMT+7 (Việt Nam)
const int   daylightOffset_sec = 0;

// ------------------- LCD I2C (0x27, 16x2) -------------------
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ------------------- RTC DS1307 -------------------
RTC_DS1307 rtc;
bool rtcFound = false;

// ------------------- NTC Temperature Sensor -------------------
const int   ADC_PIN        = A0;       // Chân Analog A0 của ESP8266
const float VCC_SENSOR     = 3.3f;     // NTC VCC (3.3V)
const float ADC_VREF       = 3.3f;     // NodeMCU A0 max input 3.3V
const float ADC_RES        = 1023.0f;  // ESP8266 ADC 10-bit (0 - 1023)
const float NTC_R0         = 10000.0f; // NTC 10kΩ ở 25°C
const float NTC_BETA       = 3950.0f;  // System Beta
const float KELVIN_OFFSET  = 273.15f;
const float T0_KELVIN      = 25.0f + KELVIN_OFFSET;

// Cache nhiệt độ
float lastTemperature = NAN;

// ------------------- Timing Settings -------------------
unsigned long lastPublish    = 0;
const unsigned long PUBLISH_INTERVAL = 10000UL;   // 10 giây
unsigned long lastLcdUpdate  = 0;
const unsigned long LCD_INTERVAL     = 1000UL;    // 1 giây
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL  = 2000UL;    // 2 giây

// ===========================================================
// Đọc nhiệt độ NTC
// ===========================================================
float readNtcTemperature() {
  long sumRaw = 0;
  const int samples = 5;
  for (int i = 0; i < samples; i++) {
    sumRaw += analogRead(ADC_PIN);
    delay(2);
  }
  float raw = (float)sumRaw / samples;

  if (raw <= 0.0f || raw >= ADC_RES) {
    return NAN;
  }

  float vAdc = (raw / ADC_RES) * ADC_VREF;
  if (vAdc <= 0.01f || vAdc >= VCC_SENSOR - 0.01f) {
    return NAN;
  }

  float rNtc = 10000.0f * (VCC_SENSOR / vAdc - 1.0f);
  if (rNtc <= 0.0f) return NAN;

  float tempK = 1.0f / (1.0f / T0_KELVIN + (1.0f / NTC_BETA) * log(rNtc / NTC_R0));
  float tempC = tempK - KELVIN_OFFSET;

  if (tempC < -40.0f || tempC > 125.0f) return NAN;
  return tempC;
}

// ===========================================================
// Kết nối WiFi
// ===========================================================
void setup_wifi() {
  Serial.print("[WiFi] Dang ket noi toi ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(250);
    Serial.print(".");
    retries++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[WiFi] Da ket noi! IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("[WiFi] Ket noi WiFi That bai. Van chay dong ho offline tu RTC.");
  }
}

// ===========================================================
// Kết nối ThingsBoard MQTT
// ===========================================================
void reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;

  if (!client.connected()) {
    Serial.print("[MQTT] Dang ket noi ThingsBoard...");
    if (client.connect("ESP8266DigitalClock", device_token, NULL)) {
      Serial.println(" Thanh cong!");
    } else {
      Serial.print(" That bai, rc=");
      Serial.println(client.state());
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {}

// ===========================================================
// Đồng bộ thời gian từ NTP -> RTC DS1307
// ===========================================================
void syncTimeFromNTP() {
  if (WiFi.status() != WL_CONNECTED) return;

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("[NTP] Dang dong bo thoi gian tu NTP...");

  time_t nowSec = time(nullptr);
  int retry = 0;
  while (nowSec < 8 * 3600 * 24L && retry < 15) {
    delay(500);
    nowSec = time(nullptr);
    retry++;
  }

  if (nowSec >= 8 * 3600 * 24L) {
    struct tm* timeinfo = localtime(&nowSec);
    Serial.printf("[NTP] Dong bo thanh cong: %02d:%02d:%02d %02d/%02d/%04d\n",
                  timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,
                  timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900);

    if (rtcFound) {
      rtc.adjust(DateTime(timeinfo->tm_year + 1900, timeinfo->tm_mon + 1,
                          timeinfo->tm_mday, timeinfo->tm_hour,
                          timeinfo->tm_min,  timeinfo->tm_sec));
      Serial.println("[RTC] Da cap nhat thoi gian NTP vao DS1307 RTC.");
    }
  } else {
    Serial.println("[NTP] Dong bo failing — dung gio hien tai cua RTC.");
  }
}

// ===========================================================
// Setup
// ===========================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== IoT Digital Clock (ESP8266) ===");

  // 1. Khởi tạo I2C Bus duy nhất cho LCD & RTC (GPIO4 & GPIO5)
  Wire.begin(D2, D1);
  delay(100);

  // 2. Khởi tạo LCD1602
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("  Dong Ho IoT   ");
  lcd.setCursor(0, 1);
  lcd.print("  Khoi dong...  ");

  // 3. Khởi tạo RTC DS1307 trên I2C chung
  if (!rtc.begin(&Wire)) {
    Serial.println("[ERROR] Khong tim thay RTC DS1307 tren I2C!");
    rtcFound = false;
  } else {
    rtcFound = true;
    Serial.println("[RTC] DS1307 khoi tao thanh cong tren I2C.");
    if (!rtc.isrunning()) {
      Serial.println("[RTC] DS1307 dang dung, khoi tao lai thoi gian bien dich...");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  }

  // 4. Wi-Fi & NTP
  setup_wifi();
  syncTimeFromNTP();

  // 5. MQTT Client setup
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  lcd.clear();
  Serial.println("[SYSTEM] He thong ESP8266 san sang chay.");
}

// ===========================================================
// Hiển thị LCD
// ===========================================================
void updateLCD(const DateTime& now, float temperature) {
  char line0[17];
  snprintf(line0, sizeof(line0), "%02d:%02d:%02d  %02d/%02d",
           now.hour(), now.minute(), now.second(),
           now.day(), now.month());
  lcd.setCursor(0, 0);
  lcd.print(line0);

  lcd.setCursor(0, 1);
  if (isnan(temperature)) {
    lcd.print("Nhiet: ---      ");
  } else {
    char line1[17];
    snprintf(line1, sizeof(line1), "Nhiet: %5.1f%cC   ", temperature, (char)223);
    lcd.print(line1);
  }
}

// ===========================================================
// Gửi MQTT Telemetry
// ===========================================================
void publishTelemetry(const DateTime& now, float temperature) {
  if (WiFi.status() != WL_CONNECTED) return;
  if (!client.connected()) return;

  char payload[128];
  if (isnan(temperature)) {
    snprintf(payload, sizeof(payload),
             "{\"time\":\"%02d:%02d:%02d\",\"date\":\"%02d/%02d/%04d\"}",
             now.hour(), now.minute(), now.second(),
             now.day(),  now.month(),  now.year());
  } else {
    snprintf(payload, sizeof(payload),
             "{\"temperature\":%.1f,\"time\":\"%02d:%02d:%02d\",\"date\":\"%02d/%02d/%04d\"}",
             temperature,
             now.hour(), now.minute(), now.second(),
             now.day(),  now.month(),  now.year());
  }

  if (client.publish(mqtt_topic, payload)) {
    Serial.print("[MQTT] Telemetry: ");
    Serial.println(payload);
  } else {
    Serial.println("[MQTT] Telemetry gui that bai.");
  }
}

// ===========================================================
// Loop
// ===========================================================
void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      reconnectMQTT();
    }
    client.loop();
  }

  unsigned long nowMs = millis();

  // Đọc NTC sensor (mỗi 2s)
  if (nowMs - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = nowMs;
    float t = readNtcTemperature();
    if (!isnan(t)) {
      lastTemperature = t;
      Serial.printf("[NTC] Nhiet do: %.1f C\n", t);
    }
  }

  // Cập nhật LCD (mỗi 1s)
  if (nowMs - lastLcdUpdate >= LCD_INTERVAL) {
    lastLcdUpdate = nowMs;
    DateTime now;
    if (rtcFound) {
      now = rtc.now();
    } else {
      time_t tNow = time(nullptr);
      struct tm* tmInfo = localtime(&tNow);
      now = DateTime(tmInfo->tm_year + 1900, tmInfo->tm_mon + 1, tmInfo->tm_mday,
                     tmInfo->tm_hour, tmInfo->tm_min, tmInfo->tm_sec);
    }
    updateLCD(now, lastTemperature);
  }

  // Gửi Telemetry (mỗi 10s)
  if (nowMs - lastPublish >= PUBLISH_INTERVAL) {
    lastPublish = nowMs;
    DateTime now;
    if (rtcFound) {
      now = rtc.now();
    } else {
      time_t tNow = time(nullptr);
      struct tm* tmInfo = localtime(&tNow);
      now = DateTime(tmInfo->tm_year + 1900, tmInfo->tm_mon + 1, tmInfo->tm_mday,
                     tmInfo->tm_hour, tmInfo->tm_min, tmInfo->tm_sec);
    }
    publishTelemetry(now, lastTemperature);
  }
}
