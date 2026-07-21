/*
  ====================================================================
  IoT Digital Clock — ESP32  (v3 — NTC Analog Sensor)
  --------------------------------------------------------------------
  Hardware:
    - ESP32 DevKit C v4
    - LCD1602 I2C (0x27)  — hien thi gio & nhiet do
    - DS1307 RTC (I2C)    — luu gio du phong
    - NTC Temperature Sensor (analog) tren GPIO34 qua dien tro 10k
  Chuc nang:
    - Dong bo gio thuc tu NTP, cap nhat DS1307
    - Doc nhiet do tu cam bien NTC qua ADC
    - Gui telemetry (nhiet do + gio) len ThingsBoard qua MQTT
  ====================================================================
*/

#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>

// ------------------- WiFi -------------------
const char* ssid     = "Wokwi-GUEST";
const char* password = "";

// ------------------- ThingsBoard / MQTT -------------------
const char* mqtt_server  = "demo.thingsboard.io";
const int   mqtt_port    = 1883;
const char* device_token = "exzBQwFLEN6mbWHMCGh9";
const char* mqtt_topic   = "v1/devices/me/telemetry";

WiFiClient   espClient;
PubSubClient client(espClient);

// ------------------- NTP -------------------
const char* ntpServer          = "pool.ntp.org";
const long  gmtOffset_sec      = 7 * 3600;   // GMT+7 (Viet Nam)
const int   daylightOffset_sec = 0;

// ------------------- LCD I2C (0x27, 16x2) -------------------
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ------------------- RTC DS1307 -------------------
RTC_DS1307 rtc;

// ------------------- NTC Temperature Sensor -------------------
// Mach chia ap: 3.3V ─── R_FIXED(10kΩ) ─── GPIO34 ─── NTC ─── GND
// Khi nhiet do tang, dien tro NTC giam, dien ap tai GPIO34 giam.
const int   ADC_PIN    = 34;       // GPIO34 (input-only ADC pin)
const float VCC_MV     = 3300.0f; // 3.3 V tinh theo mV
const float ADC_RES    = 4095.0f; // 12-bit ADC (0-4095)
const float R_FIXED    = 10000.0f; // Dien tro co dinh 10 kΩ
const float NTC_R0     = 10000.0f; // Dien tro NTC tai 25°C = 10 kΩ
const float NTC_BETA   = 3950.0f;  // He so Beta (B25/85) cua NTC
const float KELVIN     = 273.15f;
const float T0_KELVIN  = 25.0f + KELVIN;  // 25°C tinh theo Kelvin

// Cache gia tri cam bien
float lastTemperature = NAN;

// ------------------- Timing -------------------
unsigned long lastPublish    = 0;
const unsigned long PUBLISH_INTERVAL = 10000UL;   // 10s

unsigned long lastLcdUpdate  = 0;
const unsigned long LCD_INTERVAL     = 1000UL;    // 1s

unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL  = 2000UL;    // 2s

// ===========================================================
// Doc nhiet do tu NTC qua cong thuc Steinhart-Hart (phuong
// trinh Beta): 1/T = 1/T0 + (1/B) * ln(R/R0)
// ===========================================================
float readNtcTemperature() {
  int raw = analogRead(ADC_PIN);
  if (raw <= 0 || raw >= (int)ADC_RES) {
    return NAN; // Tranh chia cho 0
  }

  // Tinh dien ap tai ADC pin (mV)
  float vAdc_mV = (raw / ADC_RES) * VCC_MV;

  // Tinh dien tro NTC: R_NTC = R_FIXED * V_adc / (VCC - V_adc)
  // (NTC o phia duoi, R_FIXED o phia tren)
  float rNtc = R_FIXED * vAdc_mV / (VCC_MV - vAdc_mV);

  // Steinhart-Hart (Beta equation):
  float tempK = 1.0f / (1.0f / T0_KELVIN + (1.0f / NTC_BETA) * log(rNtc / NTC_R0));
  float tempC = tempK - KELVIN;

  // Loc gia tri phi thuc te
  if (tempC < -40.0f || tempC > 125.0f) return NAN;
  return tempC;
}

// ===========================================================
void setup_wifi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected, IP: ");
  Serial.println(WiFi.localIP());
}

// ===========================================================
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to ThingsBoard MQTT...");
    if (client.connect("ESP32DigitalClock", device_token, NULL)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" — retry in 5s");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  // Xu ly lenh tu ThingsBoard (neu can)
}

// ===========================================================
void syncTimeFromNTP() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Syncing time with NTP...");

  struct tm timeinfo;
  int retry = 0;
  while (!getLocalTime(&timeinfo) && retry < 10) {
    Serial.println("Waiting for NTP sync...");
    delay(1000);
    retry++;
  }

  if (getLocalTime(&timeinfo)) {
    rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1,
                        timeinfo.tm_mday, timeinfo.tm_hour,
                        timeinfo.tm_min,  timeinfo.tm_sec));
    Serial.println("Time synced and RTC updated.");
  } else {
    Serial.println("NTP sync failed — using RTC time.");
  }
}

// ===========================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  // I2C bus (SDA=21, SCL=22)
  Wire.begin(21, 22);
  delay(100);

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("  Dong Ho IoT   ");
  lcd.setCursor(0, 1);
  lcd.print("  Khoi dong...  ");

  // RTC
  if (!rtc.begin()) {
    Serial.println("[ERROR] Khong tim thay DS1307!");
  } else {
    Serial.println("RTC OK.");
    if (!rtc.isrunning()) {
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  }

  // ADC
  analogReadResolution(12);   // 12-bit: 0-4095
  // Ghi chu: trong Wokwi, dai dien ap ADC mac dinh la 0-3.3V (11dB)
  Serial.println("NTC sensor on GPIO34 ready.");

  // WiFi + NTP
  setup_wifi();
  syncTimeFromNTP();

  // MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  lcd.clear();
  Serial.println("System ready.");
}

// ===========================================================
void updateLCD(const DateTime& now, float temperature) {
  // Dong 0: HH:MM:SS  DD/MM/YY
  char line0[17];
  snprintf(line0, sizeof(line0), "%02d:%02d:%02d %02d/%02d",
           now.hour(), now.minute(), now.second(),
           now.day(), now.month());
  lcd.setCursor(0, 0);
  lcd.print(line0);

  // Dong 1: Nhiet do
  lcd.setCursor(0, 1);
  if (isnan(temperature)) {
    lcd.print("Nhiet do: ---   ");
  } else {
    char line1[17];
    // Ky tu 0xDF = ° tren LCD ROM A00
    snprintf(line1, sizeof(line1), "Nhiet: %5.1f%cC  ", temperature, (char)223);
    lcd.print(line1);
  }
}

// ===========================================================
void publishTelemetry(const DateTime& now, float temperature) {
  if (isnan(temperature)) {
    Serial.println("[MQTT] Skip — nhiet do khong hop le.");
    return;
  }

  char payload[128];
  snprintf(payload, sizeof(payload),
           "{\"temperature\":%.1f,\"time\":\"%02d:%02d:%02d\",\"date\":\"%02d/%02d/%04d\"}",
           temperature,
           now.hour(), now.minute(), now.second(),
           now.day(),  now.month(),  now.year());

  if (client.publish(mqtt_topic, payload)) {
    Serial.print("[MQTT] Published: ");
    Serial.println(payload);
  } else {
    Serial.println("[MQTT] Publish failed.");
  }
}

// ===========================================================
void loop() {
  // Giu ket noi WiFi
  if (WiFi.status() != WL_CONNECTED) {
    setup_wifi();
  }

  // Giu ket noi MQTT
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  unsigned long nowMs = millis();

  // Doc cam bien NTC moi 2s
  if (nowMs - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = nowMs;
    float t = readNtcTemperature();
    if (!isnan(t)) {
      lastTemperature = t;
      Serial.print("[NTC] Nhiet do: ");
      Serial.print(t, 1);
      Serial.println(" C");
    } else {
      Serial.println("[NTC] Doc that bai hoac ngoai dai.");
    }
  }

  // Cap nhat LCD moi 1s
  if (nowMs - lastLcdUpdate >= LCD_INTERVAL) {
    lastLcdUpdate = nowMs;
    DateTime now = rtc.now();
    updateLCD(now, lastTemperature);
  }

  // Gui MQTT moi 10s
  if (nowMs - lastPublish >= PUBLISH_INTERVAL) {
    lastPublish = nowMs;
    DateTime now = rtc.now();
    publishTelemetry(now, lastTemperature);
  }
}
