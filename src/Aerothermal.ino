// Automatic Aerothermal System - ESP32
// === ESP32 Fan Controller v1.8 - RPM DISPLAY FIX ===

#define BLYNK_TEMPLATE_ID "TMPL3GUIT4B7w"
#define BLYNK_TEMPLATE_NAME "Aerothermal"
#define BLYNK_AUTH_TOKEN "Xw0Ez9yw-4D_IFHzxqTy7ZKqivl1XxPM"

#include <Arduino.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------------------- USER CONFIG ----------------------
const char* wifi_ssid = "Airtel_9925222069_5G";  // ADD YOUR SSID
const char* wifi_password = "vansh0918";   // ADD YOUR PASSWORD
// ---------------------------------------------------------

// Hardware pins
const int TEMP_SENSOR_PIN = 23;
const int PWM_FAN_PIN     = 4;
const int TACH_PIN        = 5;
const int OLED_SDA        = 21;
const int OLED_SCL        = 22;

// OLED settings
const int OLED_WIDTH   = 128;
const int OLED_HEIGHT  = 64;
const uint8_t OLED_ADDRESS = 0x3C;

// PWM settings
const int LEDC_CHANNEL  = 0;
const int FAN_PWM_FREQ  = 25000;
const int FAN_PWM_BITS  = 8;
const int PWM_MIN       = 80;
const int PWM_MAX       = 255;

// RPM sampling
const unsigned long RPM_SAMPLE_MS = 1000;

// *** NEW: pulses per revolution for your fan ***
const uint8_t PULSES_PER_REV = 2;   // try 2 first; if wrong, test 1 or 4

// Temperature thresholds
const float TEMP_COOL   = 25.0f;
const float TEMP_NORMAL = 35.0f;
const float TEMP_HOT    = 50.0f;
const float TEMP_ERROR  = -127.0f;

// Blynk virtual pins
const int VPIN_TEMP        = V0;
const int VPIN_RPM         = V1;
const int VPIN_PWM_SLIDER  = V2;
const int VPIN_PWM_DISPLAY = V3;
const int VPIN_AUTO_MODE   = V4;
const int VPIN_FAN_STATUS  = V5;

// Objects
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature tempSensor(&oneWire);
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

// Volatile counters
volatile unsigned long tachPulseCount = 0;
volatile unsigned long totalPulses    = 0;

// System state
float currentTemperature = 25.0f;
unsigned long currentRPM = 0;
int currentPWM           = PWM_MIN;
bool autoMode            = true;

// Diagnostics flags
bool oledWorking      = false;
bool tempSensorWorking= false;
bool fanControlWorking= false;
bool wifiConnected    = false;
bool tachConnected    = false;

// Timing
unsigned long lastRpmCalc     = 0;
unsigned long lastTempRead    = 0;
unsigned long lastBlynkUpdate = 0;
unsigned long lastOLEDUpdate  = 0;

// ---------------------- ISR ----------------------
void IRAM_ATTR tachISR() {
  static unsigned long lastMicros = 0;
  unsigned long now = micros();
  if (now - lastMicros > 200) {        // ~200 us debounce
    tachPulseCount++;
    totalPulses++;
    tachConnected = true;
    lastMicros = now;
  }
}

// ---------------------- Utility ----------------------
int tempToPWM(float temp) {
  if (temp <= TEMP_ERROR) return PWM_MIN;
  if (temp <= TEMP_COOL)  return PWM_MIN;

  if (temp <= TEMP_NORMAL) {
    float ratio = (temp - TEMP_COOL) / (TEMP_NORMAL - TEMP_COOL);
    return PWM_MIN + (int)(ratio * (PWM_MAX - PWM_MIN) * 0.6f);
  }

  if (temp <= TEMP_HOT) {
    float ratio = (temp - TEMP_NORMAL) / (TEMP_HOT - TEMP_NORMAL);
    return PWM_MIN
         + (int)(0.6f * (PWM_MAX - PWM_MIN))
         + (int)(ratio * (PWM_MAX - PWM_MIN) * 0.4f);
  }

  return PWM_MAX;
}

// ---------------------- Initialization ----------------------
bool initOLED() {
  Wire.begin(OLED_SDA, OLED_SCL);
  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    oledWorking = true;
    Serial.println("OLED initialized at 0x3C");
    return true;
  }
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
    oledWorking = true;
    Serial.println("OLED initialized at 0x3D");
    return true;
  }
  oledWorking = false;
  Serial.println("ERROR: OLED initialization failed!");
  return false;
}

bool initTemp() {
  Serial.println("Initializing DS18B20...");
  delay(500);

  tempSensor.begin();
  int deviceCount = tempSensor.getDeviceCount();
  Serial.print("Found ");
  Serial.print(deviceCount);
  Serial.println(" DS18B20(s)");

  if (deviceCount == 0) {
    Serial.println("ERROR: No DS18B20! Check wiring & 4.7k pullup");
    tempSensorWorking = false;
    return false;
  }

  tempSensor.setResolution(12);

  for (int attempt = 0; attempt < 3; attempt++) {
    delay(100);
    tempSensor.requestTemperatures();
    delay(750);

    float t = tempSensor.getTempCByIndex(0);
    Serial.print("Attempt ");
    Serial.print(attempt + 1);
    Serial.print(": ");
    Serial.print(t);
    Serial.println(" C");

    if (t != DEVICE_DISCONNECTED_C && t > TEMP_ERROR && t < 85.0f) {
      tempSensorWorking = true;
      currentTemperature = t;
      Serial.println("Temperature sensor OK!");
      return true;
    }
  }

  Serial.println("ERROR: Temp sensor timeout!");
  tempSensorWorking = false;
  return false;
}

bool initFanPWM() {
  ledcSetup(LEDC_CHANNEL, FAN_PWM_FREQ, FAN_PWM_BITS);
  ledcAttachPin(PWM_FAN_PIN, LEDC_CHANNEL);
  ledcWrite(LEDC_CHANNEL, currentPWM);
  fanControlWorking = true;
  Serial.println("Fan PWM initialized");
  return true;
}

bool initWiFiAndBlynk() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_password);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(250);
  }
  wifiConnected = (WiFi.status() == WL_CONNECTED);
  if (wifiConnected) {
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
    Blynk.config(BLYNK_AUTH_TOKEN);
    if (!Blynk.connect(3000)) {
      Serial.println("Blynk timeout (will retry)");
    } else {
      Serial.println("Blynk connected");
    }
    return true;
  }
  Serial.println("WiFi failed");
  return false;
}

// ---------------------- Blynk handlers ----------------------
BLYNK_WRITE(V2) {
  if (!autoMode) {
    int newPWM = param.asInt();
    newPWM = constrain(newPWM, PWM_MIN, PWM_MAX);
    currentPWM = newPWM;
    if (fanControlWorking) ledcWrite(LEDC_CHANNEL, currentPWM);
    Serial.printf("Manual PWM -> %d\n", currentPWM);
  }
}

BLYNK_WRITE(V4) {
  autoMode = (param.asInt() == 1);
  Serial.println(autoMode ? "AUTO mode" : "MANUAL mode");
}

// ---------------------- Display ----------------------
void updateOLED() {
  if (!oledWorking) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // Temperature
  display.setCursor(0,0);
  display.print("Temp: ");
  if (tempSensorWorking && currentTemperature > TEMP_ERROR) {
    display.print(currentTemperature, 1);
    display.print(" C");
  } else {
    display.print("ERR");
  }

  // PWM
  display.setCursor(0,12);
  display.print("PWM: ");
  display.print(currentPWM);
  display.print(" (");
  display.print((currentPWM * 100) / 255);
  display.print("%)");

  // RPM
  display.setCursor(0,24);
  display.print("RPM: ");
  if (tachConnected && currentRPM > 0) {
    display.print(currentRPM);
  } else {
    display.print("----");
  }

  // Mode
  display.setCursor(0,36);
  display.print("Mode: ");
  display.print(autoMode ? "Auto" : "Manual");

  // WiFi
  display.setCursor(0,48);
  display.print("WiFi: ");
  display.print(wifiConnected ? "OK" : "OFF");

  // Status
  display.setCursor(0,56);
  if (!tachConnected) {
    display.print("TACH: No signal");
  } else if (!tempSensorWorking) {
    display.print("TEMP: Check wire");
  } else {
    display.print("All OK");
  }

  display.display();
}

// ---------------------- Setup ----------------------
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== ESP32 Fan Controller v1.8 ===");

  oledWorking       = initOLED();
  tempSensorWorking = initTemp();
  fanControlWorking = initFanPWM();

  if (oledWorking) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("ESP32 Fan v1.8");
    display.println("");
    display.print("OLED: "); display.println(oledWorking ? "OK" : "FAIL");
    display.print("Temp: "); display.println(tempSensorWorking ? "OK" : "FAIL");
    display.print("Fan:  "); display.println(fanControlWorking ? "OK" : "FAIL");
    display.print("WiFi: Connecting...");
    display.display();
  }

  pinMode(TACH_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TACH_PIN), tachISR, FALLING);
  Serial.println("Tach interrupt on GPIO 5");

  wifiConnected = initWiFiAndBlynk();

  if (oledWorking) {
    display.setCursor(0,56);
    display.print("WiFi: ");
    display.println(wifiConnected ? "OK" : "FAIL");
    display.display();
  }

  lastRpmCalc     = millis();
  lastTempRead    = millis();
  lastBlynkUpdate = millis();
  lastOLEDUpdate  = millis();

  Serial.println("Setup complete!");
}

// ---------------------- Loop ----------------------
void loop() {
  if (wifiConnected) {
    Blynk.run();
  } else {
    static unsigned long lastTry = 0;
    if (millis() - lastTry > 10000) {
      lastTry = millis();
      if (WiFi.status() != WL_CONNECTED) {
        WiFi.begin(wifi_ssid, wifi_password);
      } else {
        Blynk.connect(3000);
      }
    }
  }

  unsigned long now = millis();

  // Temperature (every 2s)
  if (now - lastTempRead >= 2000) {
    if (tempSensorWorking) {
      tempSensor.requestTemperatures();
      delay(100);
      float t = tempSensor.getTempCByIndex(0);
      if (t != DEVICE_DISCONNECTED_C && t > TEMP_ERROR && t < 85.0f) {
        currentTemperature = t;
        if (autoMode) {
          int newPWM = tempToPWM(currentTemperature);
          if (newPWM != currentPWM) {
            currentPWM = newPWM;
            if (fanControlWorking) ledcWrite(LEDC_CHANNEL, currentPWM);
          }
        }
      } else {
        Serial.println("Warning: invalid temp");
        tempSensorWorking = initTemp();
      }
    }
    lastTempRead = now;
  }

  // RPM (every 1s) - CORRECTED CALCULATION
  if (now - lastRpmCalc >= RPM_SAMPLE_MS) {
    noInterrupts();
    unsigned long pulses = tachPulseCount;
    tachPulseCount = 0;
    interrupts();

    // correct: 2 pulses per rev (change if your fan is different)
    if (PULSES_PER_REV > 0) {
      currentRPM = (pulses * 60UL) / PULSES_PER_REV;
    } else {
      currentRPM = 0;
    }

    Serial.printf("Pulses:%lu | RPM:%lu | PWM:%d | Temp:%.1f\n",
                  pulses, currentRPM, currentPWM, currentTemperature);

    lastRpmCalc = now;
  }

  // OLED (every 300ms)
  if (now - lastOLEDUpdate >= 300) {
    updateOLED();
    lastOLEDUpdate = now;
  }

  // Blynk (every 3s)
  if (wifiConnected && Blynk.connected() && (now - lastBlynkUpdate >= 3000)) {
    Blynk.virtualWrite(VPIN_TEMP,        currentTemperature);
    Blynk.virtualWrite(VPIN_RPM,         currentRPM);
    Blynk.virtualWrite(VPIN_PWM_DISPLAY, currentPWM);
    Blynk.virtualWrite(VPIN_FAN_STATUS,  String(currentRPM) + " RPM");
    Blynk.virtualWrite(VPIN_AUTO_MODE,   autoMode ? 1 : 0);
    lastBlynkUpdate = now;
  }

  delay(10);
}
