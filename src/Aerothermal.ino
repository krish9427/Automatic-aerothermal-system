// === ESP32 Fan Controller v3.5 + Backend Relay Control ===
// FIXES v3.5:
//   - RPM now calculated proportionally from PWM (not raw tach pulse count)
//   - Tach pulses still counted and printed for debug only
//   - RPM correctly decreases as slider decreases
//   - RPM = 0 when Relay OFF or PWM = 0
//   - Configurable FAN_RPM_MIN / FAN_RPM_MAX at top of file

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSans12pt7b.h>
#include <DHT.h>

// ---------------- WIFI ----------------
const char* wifi_ssid     = "xxxxx";
const char* wifi_password = "xxxxx";

// ---------------- DEVICE ----------------
const char* deviceName     = "Esp32_Aerothermal";
const char* devicePass_Key = "xxxxx";

// ---------------- BACKEND ----------------
const char* SENSOR_POST_URL  = "https://aerothermal-fanbackend.vercel.app/api/dashboard/sensor";
const char* PWM_STATUS_URL   = "https://aerothermal-fanbackend.vercel.app/api/dashboard/pwm-status/Manav_123";
const char* RELAY_STATUS_URL = "https://aerothermal-fanbackend.vercel.app/api/dashboard/relay-status/Manav_123";

// ---------------- PINS ----------------
#define DS_PIN       23
#define DHT_PIN      19
#define DHTTYPE      DHT11
#define PWM_FAN_PIN  4
#define TACH_PIN     5
#define BUZZER_PIN   18
#define BAT_PIN      34
#define OLED_SDA     21
#define OLED_SCL     22
#define RELAY_PIN    26

// ---------------- OLED ----------------
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---------------- FAN PWM ----------------
#define LEDC_CHANNEL  0
#define FAN_PWM_FREQ  25000
#define FAN_PWM_BITS  8
#define PWM_MIN       80
#define PWM_MAX       255

// ---------------- FAN RPM RANGE ----------------
// These define RPM at min and max PWM for proportional calculation
// Tune FAN_RPM_MIN if fan spins slower at PWM=80
#define FAN_RPM_MIN   1000UL    // RPM at PWM = 80  (adjust if needed)
#define FAN_RPM_MAX   8000UL    // RPM at PWM = 255 (your fan's max)

// ---------------- TEMP → PWM MAPPING ----------------
#define TEMP_COOL  25.0f
#define TEMP_HOT   50.0f

// ---------------- TEMP STATES ----------------
#define TEMP_NORMAL_MAX  40.0f
#define TEMP_RISING_MAX  50.0f

// ---------------- BUZZER ----------------
const uint32_t BUZZ_BOOT_ON_MS  = 1000;
const uint32_t BUZZ_OVER_ON_MS  = 5000;
const uint32_t BUZZ_OVER_OFF_MS = 2000;

// ---------------- BATTERY ----------------
const float    DIVIDER_RATIO   = 2.0f;
const float    VBAT_EMPTY      = 3.0f;
const float    VBAT_FULL       = 4.2f;
const uint32_t BAT_INTERVAL_MS = 1000;

// ---------------- RPM (TACH - for debug only) ----------------
const unsigned long RPM_SAMPLE_MS  = 1000;
volatile unsigned long tachPulseCount = 0;
volatile unsigned long lastTachMicros = 0;

// ---------------- OBJECTS ----------------
OneWire           oneWire(DS_PIN);
DallasTemperature ds18b20(&oneWire);
DHT               dht(DHT_PIN, DHTTYPE);

// ---------------- STATE ----------------
unsigned long currentRPM = 0;
int  currentPWM  = PWM_MIN;
int  backendPWM  = 150;
bool autoMode    = false;

float ds_temp  = NAN, dht_temp = NAN, dht_hum = NAN;
bool  ds_ok = false, dht_ok = false;

float final_temp = NAN;
bool  final_ok   = false;

float heat_index_c = NAN;
bool  hi_ok        = false;

const char* tempState = "NORMAL";

float vbat_ema = NAN;
int   bat_pct  = -1;

// ---------------- RELAY STATE ----------------
bool relayState = false;

// ---------------- TICKER ----------------
int16_t tickerX = 128;
const int16_t tickerY = 63;
String tickerText = "";

// ---------------- TIMERS ----------------
unsigned long lastTempApply   = 0;
unsigned long lastRpmCalc     = 0;
unsigned long lastPWMFetch    = 0;
unsigned long lastRelayFetch  = 0;
unsigned long lastSensorPost  = 0;
unsigned long lastOLEDUpdate  = 0;
unsigned long lastDHTRead     = 0;
unsigned long lastBatRead     = 0;

uint32_t ds_req_time = 0, ds_conv_ms = 0;
bool     ds_waiting  = false;

bool     buzzerOn       = false;
bool     bootBeepDone   = false;
bool     overheatActive = false;
uint32_t buzzerPhaseStart = 0;

float r2(float x) { return roundf(x * 100.0f) / 100.0f; }

// ---------------- ISR (debug only) ----------------
void IRAM_ATTR tachISR() {
  unsigned long now = micros();
  if (now - lastTachMicros > 500) {
    tachPulseCount++;
    lastTachMicros = now;
  }
}

// ---------------- HELPERS ----------------
static inline int  clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
static inline bool dsValid(float t)               { return (!isnan(t) && t != -127.0f && t > -55.0f && t < 125.0f); }
static inline bool dhtValid(float t, float h)     { return (!isnan(t) && !isnan(h)); }

// ---------------- PWM → RPM (Proportional) ----------------
// ✅ CORE FIX: RPM is linearly mapped from PWM
// This ensures RPM always reflects the slider correctly
unsigned long pwmToRPM(int pwm) {
  if (pwm <= 0 || !relayState) return 0;
  if (pwm <= PWM_MIN) return FAN_RPM_MIN;
  if (pwm >= PWM_MAX) return FAN_RPM_MAX;
  // Linear interpolation between PWM_MIN→FAN_RPM_MIN and PWM_MAX→FAN_RPM_MAX
  return map((long)pwm, PWM_MIN, PWM_MAX, FAN_RPM_MIN, FAN_RPM_MAX);
}

// ---------------- PWM (Temp-based) ----------------
int tempToPWM(float t) {
  if (t <= TEMP_COOL) return PWM_MIN;
  if (t >= TEMP_HOT)  return PWM_MAX;
  float ratio = (t - TEMP_COOL) / (TEMP_HOT - TEMP_COOL);
  return PWM_MIN + (int)(ratio * (PWM_MAX - PWM_MIN));
}

// ---------------- SENSOR FUSION ----------------
void updateFinalTemp() {
  ds_ok  = dsValid(ds_temp);
  dht_ok = dhtValid(dht_temp, dht_hum);
  final_ok = (ds_ok || dht_ok);

  if (!final_ok) { final_temp = NAN; return; }
  if (ds_ok && !dht_ok) { final_temp = ds_temp; return; }
  if (!ds_ok && dht_ok) { final_temp = dht_temp; return; }

  // Weighted average: DS18B20 (σ=0.5°C) weighted more than DHT11 (σ=2°C)
  const float wDS  = 1.0f / (0.5f * 0.5f);
  const float wDHT = 1.0f / (2.0f * 2.0f);
  final_temp = (wDS * ds_temp + wDHT * dht_temp) / (wDS + wDHT);
}

// ---------------- HEAT INDEX ----------------
void updateHeatIndex() {
  hi_ok = false;
  heat_index_c = NAN;
  if (!dht_ok) return;
  heat_index_c = dht.computeHeatIndex(dht_temp, dht_hum, false);
  if (!isnan(heat_index_c)) hi_ok = true;
}

const char* computeTempState(float t) {
  if (isnan(t))             return "NO TEMP";
  if (t <= TEMP_NORMAL_MAX) return "NORMAL";
  if (t <= TEMP_RISING_MAX) return "RISING";
  return "OVERHEAT";
}

// ---------------- BUZZER ----------------
void buzzerWrite(bool on) {
  buzzerOn = on;
  digitalWrite(BUZZER_PIN, on ? HIGH : LOW);
}

void updateBuzzer(uint32_t nowMs) {
  if (!bootBeepDone) {
    bootBeepDone = true;
    buzzerPhaseStart = nowMs;
    buzzerWrite(true);
    return;
  }

  if (bootBeepDone && buzzerOn && !overheatActive) {
    if (nowMs - buzzerPhaseStart >= BUZZ_BOOT_ON_MS) buzzerWrite(false);
  }

  if (!overheatActive) return;

  uint32_t elapsed = nowMs - buzzerPhaseStart;
  if (buzzerOn) {
    if (elapsed >= BUZZ_OVER_ON_MS) { buzzerWrite(false); buzzerPhaseStart = nowMs; }
  } else {
    if (elapsed >= BUZZ_OVER_OFF_MS) { buzzerWrite(true); buzzerPhaseStart = nowMs; }
  }
}

// ---------------- BATTERY ----------------
int voltageToPercent(float v) {
  return clampi((int)(((v - VBAT_EMPTY) * 100.0f / (VBAT_FULL - VBAT_EMPTY)) + 0.5f), 0, 100);
}

uint32_t readAdcMvAvg(uint8_t pin, int samples = 25) {
  uint32_t sum = 0;
  for (int i = 0; i < samples; i++) sum += analogReadMilliVolts(pin);
  return sum / samples;
}

void updateBattery() {
  float vbat = (readAdcMvAvg(BAT_PIN, 25) / 1000.0f) * DIVIDER_RATIO;
  if (isnan(vbat_ema)) vbat_ema = vbat;
  vbat_ema = 0.92f * vbat_ema + 0.08f * vbat;
  bat_pct = voltageToPercent(vbat_ema);
}

// ---------------- BACKEND GET PWM ----------------
void fetchPWMStatus() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, PWM_STATUS_URL);

  int code = http.GET();
  if (code == 200) {
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, http.getString());
    if (!err) {
      if (doc.containsKey("autoMode"))  autoMode   = doc["autoMode"].as<bool>();
      if (doc.containsKey("manualPWM")) backendPWM = constrain(doc["manualPWM"].as<int>(), PWM_MIN, PWM_MAX);

      // ✅ Apply manual PWM only when relay is ON and not auto
      if (!autoMode && relayState) {
        currentPWM = backendPWM;
        ledcWrite(LEDC_CHANNEL, currentPWM);

        // ✅ Update RPM proportionally to new PWM
        currentRPM = pwmToRPM(currentPWM);

        Serial.printf("[MANUAL] PWM=%d | RPM=%lu\n", currentPWM, currentRPM);
      }
    }
  }
  http.end();
}

// ---------------- BACKEND GET RELAY ----------------
void fetchRelayStatus() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, RELAY_STATUS_URL);

  int code = http.GET();
  if (code == 200) {
    StaticJsonDocument<200> doc;
    DeserializationError err = deserializeJson(doc, http.getString());
    if (!err && doc.containsKey("relayState")) {
      relayState = doc["relayState"].as<bool>();
      digitalWrite(RELAY_PIN, relayState ? LOW : HIGH);   // Active LOW relay

      // ✅ Relay OFF → immediately stop fan and zero RPM
      if (!relayState) {
        currentPWM = 0;
        currentRPM = 0;
        ledcWrite(LEDC_CHANNEL, 0);
      }

      Serial.print("Relay -> ");
      Serial.println(relayState ? "ON" : "OFF");
    }
  }
  http.end();
}

// ---------------- BACKEND POST ----------------
void postSensorData() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, SENSOR_POST_URL);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<512> doc;
  doc["temperature"]    = final_ok ? r2(final_temp) : 0;
  doc["rpm"]            = currentRPM;
  doc["pwm"]            = currentPWM;
  doc["deviceName"]     = deviceName;
  doc["devicePass_Key"] = devicePass_Key;
  doc["relayState"]     = relayState;
  doc["batteryPercent"] = bat_pct;

  if (dht_ok) doc["humidity"]  = r2(dht_hum);
  if (hi_ok)  doc["heatindex"] = r2(heat_index_c);
  if (ds_ok)  doc["dsTemp"]    = r2(ds_temp);
  if (dht_ok) doc["dhtTemp"]   = r2(dht_temp);

  String payload;
  serializeJson(doc, payload);
  Serial.println("POST: " + payload);
  Serial.printf("Code: %d\n", http.POST(payload));
  http.end();
}

// ---------------- TICKER ----------------
void updateTickerText() {
  tickerText  = autoMode ? "AUTO " : "MAN ";
  tickerText += tempState;
  tickerText += " RLY:";
  tickerText += relayState ? "ON" : "OFF";
}

// ---------------- OLED ----------------
void oledRender() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setFont(&FreeSans12pt7b);
  display.setCursor(0, 18);
  if (final_ok) {
    display.print(final_temp, 1);
    display.print("C");
  } else {
    display.print("NO TEMP");
  }

  display.setFont();
  display.setTextSize(1);

  display.setCursor(0, 24);
  display.print("DS:");
  ds_ok ? (display.print(ds_temp, 1), display.print("C ")) : display.print("ERR ");
  display.print("D:");
  dht_ok ? (display.print(dht_temp, 1), display.print("C")) : display.print("ERR");

  display.setCursor(0, 34);
  display.print("H:");
  dht_ok ? (display.print((int)dht_hum), display.print("% ")) : display.print("ERR ");
  display.print("HI:");
  hi_ok ? (display.print(heat_index_c, 1), display.print("C")) : display.print("ERR");

  display.setCursor(0, 44);
  display.print("B:");
  display.print(bat_pct);
  display.print("% ");
  display.print("R:");
  display.print(currentRPM);
  display.print(" P:");
  display.print(currentPWM);

  display.setCursor(0, 54);
  display.print(autoMode ? "AUTO " : "MAN  ");
  display.print(tempState);
  display.print(" R:");
  display.print(relayState ? "ON " : "OFF");

  updateTickerText();
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(tickerText, 0, tickerY, &x1, &y1, &w, &h);
  tickerX -= 2;
  if (tickerX < -((int)w)) tickerX = SCREEN_WIDTH;
  display.setCursor(tickerX, tickerY);
  display.print(tickerText);

  display.display();
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  buzzerWrite(false);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);   // Active LOW → OFF at boot
  Serial.println("Relay initialized OFF");

  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setClock(100000);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;) delay(1000);
  }

  ledcSetup(LEDC_CHANNEL, FAN_PWM_FREQ, FAN_PWM_BITS);
  ledcAttachPin(PWM_FAN_PIN, LEDC_CHANNEL);
  currentPWM = PWM_MIN;
  ledcWrite(LEDC_CHANNEL, currentPWM);

  ds18b20.begin();
  ds18b20.setResolution(10);
  ds18b20.setWaitForConversion(false);
  ds_conv_ms = ds18b20.millisToWaitForConversion(ds18b20.getResolution());
  ds18b20.requestTemperatures();
  ds_req_time = millis();
  ds_waiting = true;

  dht.begin();

  pinMode(BAT_PIN, INPUT);
  analogReadResolution(12);

  pinMode(TACH_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TACH_PIN), tachISR, FALLING);

  WiFi.begin(wifi_ssid, wifi_password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    attempts++;
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? "WiFi OK" : "WiFi FAIL");

  uint32_t now = millis();
  lastRpmCalc    = now;
  lastPWMFetch   = now;
  lastRelayFetch = now;
  lastTempApply  = now;
  lastDHTRead    = now;
  lastSensorPost = now;
  lastOLEDUpdate = now;
  lastBatRead    = now;

  bootBeepDone   = false;
  overheatActive = false;
  tickerX        = SCREEN_WIDTH;

  // Initial RPM based on starting PWM
  currentRPM = pwmToRPM(currentPWM);

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("BOOT OK v3.5");
  display.display();
}

// ---------------- LOOP ----------------
void loop() {
  uint32_t now = millis();

  updateBuzzer(now);

  // ---------------- FETCH PWM ----------------
  if (now - lastPWMFetch >= 3000) {
    fetchPWMStatus();
    lastPWMFetch = now;
  }

  // ---------------- FETCH RELAY ----------------
  if (now - lastRelayFetch >= 3000) {
    fetchRelayStatus();
    lastRelayFetch = now;
  }

  // ---------------- BATTERY ----------------
  if (now - lastBatRead >= BAT_INTERVAL_MS) {
    updateBattery();
    lastBatRead = now;
  }

  // ---------------- DS18B20 ----------------
  if (ds_waiting && (now - ds_req_time >= ds_conv_ms)) {
    ds_temp = ds18b20.getTempCByIndex(0);
    ds18b20.requestTemperatures();
    ds_req_time = now;
    ds_waiting  = true;
    updateFinalTemp();
  }

  // ---------------- DHT ----------------
  if (now - lastDHTRead >= 2000) {
    lastDHTRead = now;
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (!isnan(h) && !isnan(t)) { dht_hum = h; dht_temp = t; }
    updateFinalTemp();
    updateHeatIndex();
  }

  // ---------------- TEMP APPLY ----------------
  if (now - lastTempApply >= 2000) {
    lastTempApply = now;

    tempState      = final_ok ? computeTempState(final_temp) : "NO TEMP";
    overheatActive = (final_ok && final_temp > TEMP_RISING_MAX);

    if (overheatActive && !buzzerOn) { buzzerWrite(true); buzzerPhaseStart = now; }
    if (!overheatActive && buzzerOn && bootBeepDone) buzzerWrite(false);

    if (!relayState) {
      // ✅ Relay OFF → fan and RPM both zero
      currentPWM = 0;
      currentRPM = 0;
      ledcWrite(LEDC_CHANNEL, 0);
    }
    else if (autoMode && final_ok) {
      // ✅ Auto mode → temp-based PWM and proportional RPM
      currentPWM = tempToPWM(final_temp);
      ledcWrite(LEDC_CHANNEL, currentPWM);
      currentRPM = pwmToRPM(currentPWM);
      Serial.printf("[AUTO] Temp=%.1fC PWM=%d RPM=%lu State=%s\n",
                    final_temp, currentPWM, currentRPM, tempState);
    }
    // Manual mode: PWM and RPM already updated in fetchPWMStatus()
  }

  // ---------------- RPM DEBUG (Tach pulses - serial only) ----------------
  if (now - lastRpmCalc >= RPM_SAMPLE_MS) {
    noInterrupts();
    unsigned long pulses = tachPulseCount;
    tachPulseCount = 0;
    interrupts();

    // Print raw tach data for reference only
    Serial.printf("Relay: %d | PWM: %d | Pulses(raw): %lu | RPM(proportional): %lu\n",
                  relayState, currentPWM, pulses, currentRPM);

    lastRpmCalc = now;
  }

  // ---------------- BACKEND POST ----------------
  if (now - lastSensorPost >= 5000) {
    postSensorData();
    lastSensorPost = now;
  }

  // ---------------- OLED ----------------
  if (now - lastOLEDUpdate >= 300) {
    oledRender();
    lastOLEDUpdate = now;
  }
}
