#include <Arduino.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------- PIN ----------
const int LDR_PIN = 34;
const int DHT_PIN = 4;
const int LED_PIN = 18;
const int BTN_MODE_PIN = 25;
const int BTN_BRIGHTNESS_PIN = 26;
const int TRIG_PIN = 5;
const int ECHO_PIN = 17;

#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// ---------- PWM ----------
const int PWM_FREQ = 5000;
const int PWM_RESOLUTION = 8;
const int PWM_CHANNEL = 0;

// ---------- OLED ----------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
bool oledReady = false;

// ---------- INTERRUPT ----------
volatile bool modeEvent = false;
volatile bool brightnessEvent = false;

void IRAM_ATTR onModeButton() { modeEvent = true; }
void IRAM_ATTR onBrightnessButton() { brightnessEvent = true; }

// ---------- VARIABLE STATE ----------
bool autoMode = true;
const int brightnessLevels[] = {0, 64, 128, 192, 255};
const int numberOfLevels = 5;
int brightnessIndex = 2;

int currentDuty = 128;
int targetDuty = 128;

int adcValue = 0;
int distance = 0;
float temperature = NAN;
float humidity = NAN;

int oledPage = 0;
String popupMsg = "";
unsigned long popupTimer = 0;

unsigned long lastADCRead = 0;
unsigned long lastDHTRead = 0;
unsigned long lastDistanceRead = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastFadeUpdate = 0;
unsigned long lastModePress = 0;
unsigned long lastBrightnessPress = 0;

const unsigned long ADC_INTERVAL = 200;
const unsigned long DHT_INTERVAL = 2000;
const unsigned long DIST_INTERVAL = 150;
const unsigned long DISPLAY_INTERVAL = 150;
const unsigned long FADE_INTERVAL = 10;
const unsigned long DEBOUNCE_MS = 200;

// ---------- DUKUNGAN PWM ----------
void writePWM(int duty) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(LED_PIN, duty);
#else
  ledcWrite(PWM_CHANNEL, duty);
#endif
}

void initPWM() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(LED_PIN, PWM_FREQ, PWM_RESOLUTION);
#else
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(LED_PIN, PWM_CHANNEL);
#endif
  writePWM(currentDuty);
}

// ---------- BACA SENSOR ----------
int readLDR() {
  long total = 0;
  for (int i = 0; i < 8; i++) total += analogRead(LDR_PIN);
  return total / 8;
}

int readDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH, 25000); // Timeout 25ms
  if (duration == 0) return 999;
  return duration * 0.0343 / 2;
}

void showPopup(String msg) {
  popupMsg = msg;
  popupTimer = millis();
}

// ---------- LOGIKA KECERAHAN & HEMAT DAYA ----------
void updateTargetBrightness() {
  // Fitur Auto-Off saat meninggalkan meja (jarak > 100 cm)
  if (distance > 100) {
    targetDuty = 0;
    return;
  }

  if (autoMode) {
    targetDuty = map(adcValue, 0, 4095, 255, 10);
    targetDuty = constrain(targetDuty, 10, 255);
  } else {
    targetDuty = brightnessLevels[brightnessIndex];
  }
}

void handleSmoothFade() {
  if (currentDuty != targetDuty) {
    if (currentDuty < targetDuty) currentDuty++;
    else currentDuty--;
    writePWM(currentDuty);
  }
}

void processButtons() {
  bool modePressed, brightnessPressed;

  noInterrupts();
  modePressed = modeEvent;
  brightnessPressed = brightnessEvent;
  modeEvent = false;
  brightnessEvent = false;
  interrupts();

  unsigned long now = millis();

  if (modePressed && (now - lastModePress >= DEBOUNCE_MS)) {
    lastModePress = now;
    autoMode = !autoMode;
    updateTargetBrightness();
    showPopup(autoMode ? "MODE: AUTO" : "MODE: MANUAL");
  }

  if (brightnessPressed && (now - lastBrightnessPress >= DEBOUNCE_MS)) {
    lastBrightnessPress = now;
    if (!autoMode) {
      brightnessIndex = (brightnessIndex + 1) % numberOfLevels;
      updateTargetBrightness();
      int pct = map(brightnessLevels[brightnessIndex], 0, 255, 0, 100);
      showPopup("BRIGHT: " + String(pct) + "%");
    } else {
      oledPage = (oledPage + 1) % 2;
      showPopup(oledPage == 0 ? "HALAMAN 1" : "HALAMAN 2");
    }
  }
}

// ---------- TAMPILAN OLED ----------
void drawProgressBar(int x, int y, int w, int h, int val, int maxVal) {
  display.drawRect(x, y, w, h, SSD1306_WHITE);
  int fillW = map(val, 0, maxVal, 0, w - 4);
  fillW = constrain(fillW, 0, w - 4);
  display.fillRect(x + 2, y + 2, fillW, h - 4, SSD1306_WHITE);
}

void updateOLED() {
  if (!oledReady) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Standby Display saat tidak ada orang di meja
  if (distance > 100) {
    display.setTextSize(1);
    display.setCursor(30, 20);
    display.print("MODE SIAGA");
    display.setCursor(15, 35);
    display.print("Meja Kosong...");
    display.display();
    return;
  }

  // Header
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("ErgoDesk ");
  display.print(autoMode ? "[AUTO]" : "[MAN]");
  display.setCursor(110, 0);
  display.print("P");
  display.print(oledPage + 1);
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  // Halaman 1: Monitor Utama & Jarak
  if (oledPage == 0) {
    display.setCursor(0, 15);
    display.print("Jarak Duduk: ");
    display.print(distance);
    display.print(" cm");

    display.setCursor(0, 27);
    display.print("Lampu LED  : ");
    display.print(map(currentDuty, 0, 255, 0, 100));
    display.print("%");

    drawProgressBar(0, 39, 128, 9, currentDuty, 255);

    display.setCursor(0, 54);
    display.print("Suhu: ");
    if (isnan(temperature)) display.print("N/A");
    else { display.print(temperature, 1); display.print("C"); }

    display.setCursor(70, 54);
    display.print("Hum: ");
    if (isnan(humidity)) display.print("N/A");
    else { display.print((int)humidity); display.print("%"); }
  } 
  // Halaman 2: Detail Lingkungan
  else {
    display.setCursor(0, 15);
    display.print("Sensor LDR  : ");
    display.print(adcValue);

    display.setCursor(0, 28);
    display.print("Suhu Ruang  : ");
    if (isnan(temperature)) display.println("N/A");
    else { display.print(temperature, 1); display.println(" C"); }

    display.setCursor(0, 41);
    display.print("Kelembapan  : ");
    if (isnan(humidity)) display.println("N/A");
    else { display.print(humidity, 1); display.println(" %"); }
  }

  // Warning Peringatan Jarak Duduk Terlalu Dekat (<15 cm)
  if (distance < 15) {
    display.fillRect(4, 15, 120, 34, SSD1306_BLACK);
    display.drawRect(4, 15, 120, 34, SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(12, 22);
    display.print("PERINGATAN!");
    display.setCursor(10, 34);
    display.print("Jaga Jarak Mata!");
  }
  // Pop-up Tombol
  else if (millis() - popupTimer < 1200) {
    display.fillRect(14, 18, 100, 28, SSD1306_BLACK);
    display.drawRect(14, 18, 100, 28, SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(20, 28);
    display.print(popupMsg);
  }

  display.display();
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LDR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BTN_MODE_PIN, INPUT_PULLUP);
  pinMode(BTN_BRIGHTNESS_PIN, INPUT_PULLUP);

  analogReadResolution(12);
  dht.begin();
  initPWM();

  attachInterrupt(digitalPinToInterrupt(BTN_MODE_PIN), onModeButton, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTN_BRIGHTNESS_PIN), onBrightnessButton, FALLING);

  Wire.begin(21, 22);
  oledReady = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  Serial.println("\n=== ErgoDesk System Started ===");
}

// ---------- LOOP UTAMA ----------
void loop() {
  unsigned long now = millis();

  processButtons();

  // Transisi LED halus
  if (now - lastFadeUpdate >= FADE_INTERVAL) {
    lastFadeUpdate = now;
    handleSmoothFade();
  }

  // Baca Sensor Ultrasonik
  if (now - lastDistanceRead >= DIST_INTERVAL) {
    lastDistanceRead = now;
    distance = readDistance();
    updateTargetBrightness();
  }

  // Baca Sensor LDR
  if (now - lastADCRead >= ADC_INTERVAL) {
    lastADCRead = now;
    adcValue = readLDR();
    updateTargetBrightness();
  }

  // Baca DHT11
  if (now - lastDHTRead >= DHT_INTERVAL) {
    lastDHTRead = now;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t)) temperature = t;
    if (!isnan(h)) humidity = h;
  }

  // Refresh Tampilan OLED
  if (now - lastDisplayUpdate >= DISPLAY_INTERVAL) {
    lastDisplayUpdate = now;
    updateOLED();
  }
}