#include <WiFi.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Arduino.h>

// ─── USER CONFIG ──────────────────────────────────────────────────────────────
const char* WIFI_SSID     = "Lag-o-less";
const char* WIFI_PASSWORD = "7RHRapZi0";
const char* SERVER_URL    = "http://ec2-3-148-175-201.us-east-2.compute.amazonaws.com:5000/data";

// ─── PINS & THRESHOLD ─────────────────────────────────────────────────────────
#define MOISTURE_PIN 36       // analog
#define LED_PIN      25       // digital
#define BUZZER_PIN   26       // digital

const int DRY_VAL      = 3500;  // calibrate
const int WET_VAL      =  500;  // calibrate
const int DRY_PERCENT  =   40;  // warning threshold

// ─── ALERT TIMERS ──────────────────────────────────────────────────────────────
unsigned long lastBuzzerToggle = 0;
bool         buzzerState       = false;
const unsigned long BUZZER_INTERVAL = 1000;  // 1 s on/off

unsigned long lastLedToggle    = 0;
bool         ledState          = false;
const unsigned long LED_INTERVAL    = 100;   // 100 ms on/off

// ─── GLOBALS ──────────────────────────────────────────────────────────────────
TFT_eSPI      tft = TFT_eSPI();
Adafruit_AHTX0 aht;

// ─── SETUP ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // I2C for AHT20
  Wire.begin(21, 22);
  if (!aht.begin()) {
    Serial.println("AHT20 init failed! Check wiring.");
    while (1) delay(10);
  }

  // pins
  pinMode(LED_PIN,   OUTPUT);
  pinMode(BUZZER_PIN,OUTPUT);

  // TFT
  tft.init();
  tft.setRotation(1);
  tft.setTextDatum(MC_DATUM);

  // Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  tft.println("Connecting WiFi…");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
}

// ─── MAIN LOOP ────────────────────────────────────────────────────────────────
void loop() {
  // 1) read raw moisture
  int raw = analogRead(MOISTURE_PIN);
  raw = constrain(raw, WET_VAL, DRY_VAL);
  int pct = map(raw, WET_VAL, DRY_VAL, 100, 0);

  // 2) update display (only moisture)
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(6);
  tft.setTextColor(TFT_WHITE);
  tft.drawString(String(pct) + "%", tft.width()/2, tft.height()/2 - 20);
  if (pct < DRY_PERCENT) {
    tft.setTextSize(3);
    tft.setTextColor(TFT_RED);
    tft.drawString("DRY!", tft.width()/2, tft.height()/2 + 40);
  }

  // 3) LED + buzzer logic
  bool isDry = (pct < DRY_PERCENT);
  unsigned long now = millis();
  if (isDry) {
    // buzzer: long beep / long silence
    if (now - lastBuzzerToggle >= BUZZER_INTERVAL) {
      buzzerState = !buzzerState;
      lastBuzzerToggle = now;
      if (buzzerState) tone(BUZZER_PIN, 1000);
      else           noTone(BUZZER_PIN);
    }
    // LED: fast flash
    if (now - lastLedToggle >= LED_INTERVAL) {
      ledState = !ledState;
      lastLedToggle = now;
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    }
  } else {
    noTone(BUZZER_PIN);
    digitalWrite(LED_PIN, LOW);
  }

  // 4) read temp & humidity
  sensors_event_t humidity, tempEvt;
  aht.getEvent(&humidity, &tempEvt);
  float cel = tempEvt.temperature;                   // °C
  float fah = cel * 9.0 / 5.0 + 32.0;                // °F
  float hum = humidity.relative_humidity;
  
  
  Serial.printf("Temp = %.1f°F, Humidity = %.1f%%\n", fah, hum);


  // 5) send to Flask server (moisture, raw, temp, hum)
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(SERVER_URL);
    http.addHeader("Content-Type", "application/json");
    String payload = String("{\"moisture\":") + pct +
                      String(",\"raw\":")    + raw +
                      String(",\"tempF\":")  + String(fah,1) +
                      String(",\"hum\":")    + String(hum,1) +
                      String("}");
    int code = http.POST(payload);
    Serial.printf("POST %s → %d\n", payload.c_str(), code);
    http.end();
  }

  delay(5000);  // wait before next reading
}
