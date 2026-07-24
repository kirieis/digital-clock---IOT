/*
  ====================================================================
  IoT Digital Clock — NodeMCU 1.0 (ESP-12E Module) / ESP8266
  --------------------------------------------------------------------
  So do chan NodeMCU:
    - LCD1602 I2C (0x27)  — SDA: D2, SCL: D1, VCC: 3V/5V, GND: G
    - DS1307 RTC (I2C)    — SDA: D2, SCL: D1, VCC: 3V/5V, GND: G
    - DHT11               — VCC: 3V, GND: G, DATA: D4
  ====================================================================
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <time.h>

// ------------------- DHT11 Config -------------------
#define DHTPIN  D5        // Chan DATA cua DHT11 noi vao D5
#define DHTTYPE DHT11     // Loai cam bien: DHT11

DHT dht(DHTPIN, DHTTYPE);

// ------------------- WiFi Config -------------------
const char* ssid     = "FPT Dorm A";
const char* password = "ktxfptu2026";

// ------------------- ThingsBoard / MQTT Config -------------------
const char* mqtt_server  = "demo.thingsboard.io";
const int   mqtt_port    = 1883;
const char* device_token = "exzBQwFLEN6mbWHMCGh9";
const char* mqtt_topic   = "v1/devices/me/telemetry";

WiFiClient   espClient;
PubSubClient client(espClient);

// ------------------- NTP Config -------------------
const char* ntpServer     = "pool.ntp.org";
const long  gmtOffset_sec = 7 * 3600; // GMT+7
const int   daylightOffset_sec = 0;

// ------------------- I2C Pins (NodeMCU) -------------------
const int I2C_SDA = D2; // GPIO4
const int I2C_SCL = D1; // GPIO5

// ------------------- LCD 16x2 I2C -------------------
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ------------------- RTC DS1307 -------------------
RTC_DS1307 rtc;
bool rtcFound = false;

// ------------------- Cache gia tri cam bien -------------------
float lastTemperature = NAN;
float lastHumidity    = NAN;

// ------------------- Timing -------------------
unsigned long lastLcdUpdate  = 0;
unsigned long lastSensorRead = 0;
const unsigned long LCD_INTERVAL    = 1000UL;  // Cap nhat LCD moi 1 giay
const unsigned long SENSOR_INTERVAL = 2000UL;  // DHT11 can it nhat 2s giua 2 lan doc

// ===========================================================
// Doc nhiet do & do am tu DHT11
// ===========================================================
void readDHT() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    Serial.println("[DHT11] Loi doc cam bien! Kiem tra lai day noi DATA vao D5.");
    return;
  }

  lastTemperature = t;
  lastHumidity    = h;
  Serial.printf("[DHT11] Nhiet do: %.1f C, Do am: %.1f %%\n", t, h);
}

// ===========================================================
// Ket noi WiFi
// ===========================================================
void setup_wifi() {
  Serial.print("[WiFi] Dang ket noi toi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 40) {
    delay(250);
    Serial.print(".");
    retries++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[WiFi] Da ket noi! IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("[WiFi] Ket noi that bai. Chay offline tu RTC.");
  }
}

// ===========================================================
// Ket noi MQTT ThingsBoard
// ===========================================================
void reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (client.connected()) return;

  Serial.print("[MQTT] Dang ket noi ThingsBoard...");
  if (client.connect("NodeMCU_Clock", device_token, NULL)) {
    Serial.println(" Thanh cong!");
  } else {
    Serial.printf(" That bai, rc=%d\n", client.state());
  }
}

void callback(char* topic, byte* payload, unsigned int length) {}

// ===========================================================
// Dong bo thoi gian NTP -> RTC
// ===========================================================
void syncTimeFromNTP() {
  if (WiFi.status() != WL_CONNECTED) return;

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("[NTP] Dang dong bo thoi gian...");

  time_t nowSec = time(nullptr);
  for (int i = 0; i < 15 && nowSec < 86400L * 8; i++) {
    delay(500);
    nowSec = time(nullptr);
  }

  if (nowSec >= 86400L * 8) {
    struct tm* tm = localtime(&nowSec);
    Serial.printf("[NTP] Thanh cong: %02d:%02d:%02d %02d/%02d/%04d\n",
                  tm->tm_hour, tm->tm_min, tm->tm_sec,
                  tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
    if (rtcFound) {
      rtc.adjust(DateTime(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                          tm->tm_hour, tm->tm_min, tm->tm_sec));
      Serial.println("[RTC] Da cap nhat tu NTP.");
    }
  } else {
    Serial.println("[NTP] Dong bo that bai, dung gio RTC.");
  }
}

// ===========================================================
// Lay thoi gian hien tai
// ===========================================================
DateTime getCurrentTime() {
  if (rtcFound) return rtc.now();

  time_t t = time(nullptr);
  struct tm* tm = localtime(&t);
  return DateTime(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                  tm->tm_hour, tm->tm_min, tm->tm_sec);
}

// ===========================================================
// Hien thi LCD
// ===========================================================
void updateLCD(const DateTime& now, float temp, float hum) {
  // Dong 1: Gio + Ngay
  char line0[17];
  snprintf(line0, sizeof(line0), "%02d:%02d:%02d  %02d/%02d",
           now.hour(), now.minute(), now.second(),
           now.day(), now.month());
  lcd.setCursor(0, 0);
  lcd.print(line0);

  // Dong 2: Nhiet do + Do am
  lcd.setCursor(0, 1);
  char line1[17];
  if (isnan(temp) || isnan(hum)) {
    lcd.print("N:--.-C A: --.- ");
  } else {
    snprintf(line1, sizeof(line1), "N:%4.1fC A:%4.1f%%", temp, hum);
    lcd.print(line1);
  }
}

// ===========================================================
// Gui MQTT Telemetry
// ===========================================================
void publishTelemetry(const DateTime& now, float temp, float hum) {
  if (WiFi.status() != WL_CONNECTED) return;
  if (!client.connected()) return;

  char payload[256];
  if (isnan(temp) || isnan(hum)) {
    snprintf(payload, sizeof(payload),
             "{\"time\":\"%02d:%02d:%02d\",\"date\":\"%02d/%02d/%04d\"}",
             now.hour(), now.minute(), now.second(),
             now.day(), now.month(), now.year());
  } else {
    snprintf(payload, sizeof(payload),
             "{\"temperature\":%.1f,\"humidity\":%.1f,\"time\":\"%02d:%02d:%02d\",\"date\":\"%02d/%02d/%04d\"}",
             temp, hum,
             now.hour(), now.minute(), now.second(),
             now.day(), now.month(), now.year());
  }

  if (client.publish(mqtt_topic, payload)) {
    Serial.print("[MQTT] Telemetry: ");
    Serial.println(payload);
  } else {
    Serial.println("[MQTT] Gui that bai.");
  }
}

// ===========================================================
// Setup
// ===========================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== IoT Clock + DHT11 (NodeMCU ESP8266) ===");

  // 1. I2C Bus
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(100);

  // 2. LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("  Dong Ho IoT   ");
  lcd.setCursor(0, 1);
  lcd.print("  Khoi dong...  ");

  // 3. DHT11
  pinMode(DHTPIN, INPUT_PULLUP);
  dht.begin();
  Serial.println("[DHT11] Da khoi tao tren chan D5.");

  // 4. RTC DS1307
  if (!rtc.begin(&Wire)) {
    Serial.println("[RTC] Khong tim thay DS1307!");
  } else {
    rtcFound = true;
    Serial.println("[RTC] DS1307 OK.");
    if (!rtc.isrunning()) {
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  }

  // 5. WiFi & NTP
  setup_wifi();
  syncTimeFromNTP();

  // 6. MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  reconnectMQTT();

  // 7. Doc lan dau
  delay(2000); // DHT11 can 1-2 giay sau khi bat nguon
  readDHT();

  lcd.clear();
  Serial.println("[SYSTEM] San sang!");
}

// ===========================================================
// Loop
// ===========================================================
void loop() {
  // Duy tri ket noi MQTT
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) reconnectMQTT();
    client.loop();
  }

  unsigned long now = millis();

  // Doc cam bien & gui MQTT moi 2 giay
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;
    readDHT();
    publishTelemetry(getCurrentTime(), lastTemperature, lastHumidity);
  }

  // Cap nhat LCD moi 1 giay
  if (now - lastLcdUpdate >= LCD_INTERVAL) {
    lastLcdUpdate = now;
    updateLCD(getCurrentTime(), lastTemperature, lastHumidity);
  }
}
