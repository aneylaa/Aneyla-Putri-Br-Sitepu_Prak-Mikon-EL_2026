#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// --- KONFIGURASI PIN TERUPDATE ---
#define PIN_DHT          4   // Sensor Suhu & Kelembapan
#define PIN_PIR          13  // Sensor Gerak
#define PIN_POT          5   // Potensiometer
#define PIN_BUTTON       17  // Push Button
#define PIN_LED          10  // Lampu LED
#define PIN_SDA           8  // OLED SDA
#define PIN_SCL           9  // OLED SCL

// --- KONFIGURASI SPESIFIKASI ---
#define DHTTYPE          DHT11
#define SCREEN_WIDTH     128
#define SCREEN_HEIGHT    32
#define OLED_RESET       -1
#define SCREEN_ADDRESS   0x3C

// --- OBJEK DAN VARIABEL GLOBAL ---
DHT dht(PIN_DHT, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Variabel Interupsi (volatile)
volatile bool motionDetected = false;
volatile unsigned long lastMotionTime = 0;

// Variabel State Mesin
enum DisplayMode { AUTO_MODE, MANUAL_MODE };
DisplayMode currentMode = AUTO_MODE;

bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// Pewaktuan Non-Blocking
unsigned long previousMillisDHT = 0;
const long intervalDHT = 2000; // Pembacaan DHT11 setiap 2 detik

unsigned long previousMillisOLED = 0;
const long intervalOLED = 200; // Refesh OLED & Serial setiap 200ms

unsigned long previousMillisStrobe = 0;
bool strobeState = false;

// ISR (Interrupt Service Routine) untuk PIR Sensor
void IRAM_ATTR handlePirInterrupt() {
  motionDetected = true;
  lastMotionTime = millis();
}

void setup() {
  Serial.begin(115200);

  // Inisialisasi Pin Input/Output
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_LED, OUTPUT);

  // Configuration I2C Custom Pin ESP32-S3
  Wire.begin(PIN_SDA, PIN_SCL);

  // Inisialisasi OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Gagal menginisialisasi SSD1306 OLED"));
    for (;;);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();

  // Inisialisasi DHT
  dht.begin();

  // Attach Hardware Interrupt pada PIR
  attachInterrupt(digitalPinToInterrupt(PIN_PIR), handlePirInterrupt, RISING);
}

void loop() {
  unsigned long currentMillis = millis();

  // -------------------------------------------------------------
  // 1. PEMBACAAN INPUT ANALOG & DIGITAL
  // -------------------------------------------------------------
  int potRaw = analogRead(PIN_POT);
  float thresholdRH = map(potRaw, 0, 4095, 40, 80);

  // Debouncing Sakelar Tekan (Push Button) untuk Toggle Mode
  bool reading = digitalRead(PIN_BUTTON);
  if (reading != lastButtonState) {
    lastDebounceTime = currentMillis;
  }
  if ((currentMillis - lastDebounceTime) > debounceDelay) {
    static bool buttonState = HIGH;
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) { // Aktif saat ditekan
        currentMode = (currentMode == AUTO_MODE) ? MANUAL_MODE : AUTO_MODE;
      }
    }
  }
  lastButtonState = reading;

  // Pembacaan Periodik Sensor DHT11
  static float humidity = 0.0;
  static float temperature = 0.0;
  if (currentMillis - previousMillisDHT >= intervalDHT) {
    previousMillisDHT = currentMillis;
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (!isnan(h) && !isnan(t)) {
      humidity = h;
      temperature = t;
    }
  }

  // Auto-reset flag gerakan setelah 5 detik tanpa gerakan baru
  if (motionDetected && (currentMillis - lastMotionTime > 5000)) {
    motionDetected = false;
  }

  // -------------------------------------------------------------
  // 2. LOGIKA KENDALI UTAMA (LED AKTUATOR)
  // -------------------------------------------------------------
  if (humidity > thresholdRH) {
    // Strobo visual LED (berkedip interval 100ms) saat kondisi bahaya
    if (currentMillis - previousMillisStrobe >= 100) {
      previousMillisStrobe = currentMillis;
      strobeState = !strobeState;
      digitalWrite(PIN_LED, strobeState ? HIGH : LOW);
    }
  } else {
    // Kondisi Aman
    if (currentMode == MANUAL_MODE) {
      digitalWrite(PIN_LED, HIGH);
    } else {
      digitalWrite(PIN_LED, motionDetected ? HIGH : LOW);
    }
  }

  // -------------------------------------------------------------
  // 3. PEMBARUAN TAMPILAN OLED & SERIAL MONITOR (Dibatasi Timer)
  // -------------------------------------------------------------
  if (currentMillis - previousMillisOLED >= intervalOLED) {
    previousMillisOLED = currentMillis;

    display.clearDisplay();

    if (humidity > thresholdRH) {
      // --- TAMPILAN DASBOR BAHAYA ---
      display.setTextSize(1);
      display.setCursor(10, 0);
      display.print(F("! BAHAYA JAMUR !"));
      
      display.setTextSize(2);
      display.setCursor(10, 12);
      display.print(humidity, 1);
      display.print(F("% RH"));
      
      display.setTextSize(1);
      display.setCursor(90, 24);
      display.print(F("ALARM"));
    } else {
      // --- TAMPILAN DASBOR NORMAL ---
      display.setTextSize(1);
      
      // Baris 1: Kelembapan & Limit
      display.setCursor(0, 0);
      display.print(F("RH : "));
      display.print(humidity, 1);
      display.print(F("% ("));
      display.print((int)thresholdRH);
      display.print(F("%)"));

      // Baris 2: Suhu
      display.setCursor(0, 11);
      display.print(F("Temp: "));
      display.print(temperature, 1);
      display.print(F(" C"));

      // Baris 3: Status Mode & LED
      display.setCursor(0, 22);
      display.print(F("Mode: "));
      if (currentMode == MANUAL_MODE) {
        display.print(F("DISPLAY ON"));
      } else {
        display.print(motionDetected ? F("AUTO (ON)") : F("AUTO (OFF)"));
      }
    }

    // Kirim data buffer ke layar OLED
    display.display();

    // Data Serial Monitor
    Serial.print("RH: "); 
    Serial.print(humidity);
    Serial.print("% | Limit: "); 
    Serial.print((int)thresholdRH);
    Serial.print("% | Temp: "); 
    Serial.print(temperature);
    Serial.print("C | Mode: "); 
    Serial.println(currentMode == MANUAL_MODE ? "MANUAL" : "AUTO");
  }
}