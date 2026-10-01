#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include "Adafruit_SGP30.h"

// ===== WI-FI & TELEGRAM CREDENTIALS =====
const char* WIFI_SSID = "YOUR_WIFI_SSID";       
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";   
#define BOT_TOKEN "YOUR_TELEGRAM_BOT_TOKEN"
#define CHAT_ID "YOUR_TELEGRAM_CHAT_ID"

// ===== PIN CONFIGURATION =====
#define SOIL_PIN 1        
#define MOSFET_PIN 3      
#define SDA_PIN 8         
#define SCL_PIN 9         

// ===== IRRIGATION THRESHOLDS & TIMERS =====
const int DRY_THRESHOLD = 2600;       
const int WET_THRESHOLD = 2100;       

const unsigned long SENSOR_INTERVAL = 2000;    
const unsigned long PUMP_DURATION = 3000;      
const unsigned long SOAK_INTERVAL = 60000;     // 60 seconds delay between attempts

const int MAX_PUMP_ATTEMPTS = 3;      
const int SENSOR_MIN_VALID = 100;     
const int SENSOR_MAX_VALID = 4095;    

const unsigned long EMERGENCY_COOLDOWN = 1800000; // 30 minutes lockout
const unsigned long AIR_ALERT_COOLDOWN = 600000;   // 10 minutes air alert cooldown

// ===== AIR QUALITY THRESHOLDS =====
const uint16_t ECO2_WARN_THRESHOLD = 1000;  
const uint16_t TVOC_WARN_THRESHOLD = 300;   

// ===== GLOBAL OBJECTS & VARIABLES =====
Adafruit_SGP30 sgp;
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

unsigned long lastSensorRead = 0;
unsigned long lastPumpTime = 0;
unsigned long pumpStartTime = 0;
unsigned long emergencyStartTime = 0;
unsigned long lastAirAlertTime = 0;

bool isPumpActive = false;
int pumpAttempts = 0;
bool emergencyStop = false;

void sendTelegramMessage(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    bot.sendMessage(CHAT_ID, message, "");
  }
}

void connectToWiFi() {
  Serial.print("[WiFi] Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  secured_client.setInsecure(); // Skip SSL certificate validation

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected! IP address: ");
    Serial.println(WiFi.localIP());
    sendTelegramMessage("🤖 Smart Irrigation System online!");
  } else {
    Serial.println("\n[WiFi] Connection failed. Operating in offline mode.");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(MOSFET_PIN, OUTPUT);
  digitalWrite(MOSFET_PIN, LOW);

  Wire.begin(SDA_PIN, SCL_PIN);
  if (!sgp.begin()) {
    Serial.println("[-] Error: SGP30 sensor not detected!");
  } else {
    Serial.println("[+] SGP30 sensor initialized successfully.");
  }

  connectToWiFi();
  Serial.println("=== Smart Irrigation & Air Quality Node Ready ===");
}

void loop() {
  unsigned long currentMillis = millis();

  // Reconnect Wi-Fi if connection drops
  if (WiFi.status() != WL_CONNECTED && (currentMillis % 30000 == 0)) {
    WiFi.reconnect();
  }

  // 1. AUTOMATIC EMERGENCY LOCKOUT RESET
  if (emergencyStop && (currentMillis - emergencyStartTime >= EMERGENCY_COOLDOWN)) {
    Serial.println("[⏳ TIMER] Emergency cooldown finished. Retrying...");
    sendTelegramMessage("🔄 [RETRY] Emergency cooldown ended. Attempting automatic irrigation cycle...");
    emergencyStop = false;
    pumpAttempts = 0;
  }

  // 2. SENSOR POLLING & LOGIC
  if (currentMillis - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = currentMillis;

    int rawSoil = analogRead(SOIL_PIN);

    uint16_t eco2 = 0, tvoc = 0;
    bool airDataValid = sgp.IAQmeasure();
    if (airDataValid) {
      eco2 = sgp.eCO2;
      tvoc = sgp.TVOC;
    }

    Serial.printf("[%lu s] [STATUS] Soil: %d | eCO2: %d ppm | TVOC: %d ppb | Attempts: %d/%d %s\n",
                  millis() / 1000, rawSoil, eco2, tvoc, pumpAttempts, MAX_PUMP_ATTEMPTS, emergencyStop ? "[LOCKOUT]" : "");

    // Air Quality Telegram Alert
    if (airDataValid && (eco2 > ECO2_WARN_THRESHOLD || tvoc > TVOC_WARN_THRESHOLD)) {
      if (currentMillis - lastAirAlertTime >= AIR_ALERT_COOLDOWN || lastAirAlertTime == 0) {
        lastAirAlertTime = currentMillis;
        String alertMsg = "⚠️ [AIR QUALITY ALERT]\nPoor air quality detected!\n- eCO2: " + String(eco2) + " ppm\n- TVOC: " + String(tvoc) + " ppb\n💡 Please ventilate the room.";
        sendTelegramMessage(alertMsg);
      }
    }

    // Hardware safety check
    if (rawSoil < SENSOR_MIN_VALID || rawSoil > SENSOR_MAX_VALID) {
      Serial.println("[SENSOR ERROR] Invalid sensor data!");
      return;
    }

    // Reset attempt counter when moisture recovers
    if (rawSoil < WET_THRESHOLD) {
      if (pumpAttempts > 0 || emergencyStop) {
        Serial.println("[+] Soil moisture restored.");
        sendTelegramMessage("✅ [SUCCESS] Soil moisture restored. Emergency locks cleared.");
        pumpAttempts = 0;
        emergencyStop = false;
      }
    }

    // Trigger irrigation
    if (rawSoil > DRY_THRESHOLD && !isPumpActive && !emergencyStop) {
      if (currentMillis - lastPumpTime >= SOAK_INTERVAL || lastPumpTime == 0) {
        if (pumpAttempts < MAX_PUMP_ATTEMPTS) {
          pumpAttempts++;
          Serial.printf("[!] Dry soil detected. Activating pump (Attempt %d/%d)...\n", pumpAttempts, MAX_PUMP_ATTEMPTS);
          digitalWrite(MOSFET_PIN, HIGH);
          isPumpActive = true;
          pumpStartTime = currentMillis;

          if (pumpAttempts < MAX_PUMP_ATTEMPTS) {
            sendTelegramMessage("🌱 [WATERING] Soil is dry. Pump activated (Attempt " + String(pumpAttempts) + "/" + String(MAX_PUMP_ATTEMPTS) + "). Next check in 60s...");
          } else {
            sendTelegramMessage("🌱 [WATERING] Soil is dry. Pump activated (FINAL Attempt " + String(pumpAttempts) + "/" + String(MAX_PUMP_ATTEMPTS) + ")...");
          }
        } else {
          emergencyStop = true;
          emergencyStartTime = currentMillis;
          Serial.println("[EMERGENCY] Max attempts exceeded!");
          sendTelegramMessage("🚨 [EMERGENCY LOCK] 3 watering attempts failed! Check water tank. Lockout active for 30 minutes.");
        }
      }
    }
  }

  // 3. PUMP RUNTIME CONTROLLER
  if (isPumpActive) {
    if (currentMillis - pumpStartTime >= PUMP_DURATION) {
      digitalWrite(MOSFET_PIN, LOW);
      isPumpActive = false;
      lastPumpTime = currentMillis;
      Serial.println("[+] Water delivery finished.");
    }
  }
}