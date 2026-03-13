/*
 * UNIT B: ROOM RECEIVER (ESP32 + LoRa + Relay + Audio Alerts)
 * Features: Offline Safe, Auto Mode, Voice Feedback
 */

#include <WiFi.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>
#include <Arduino_MQTT_Client.h>
#include "DFRobotDFPlayerMini.h"

// ---- WIFI & THINGSBOARD CREDENTIALS ----
const char* WIFI_SSID = "Write your wifi name here";
const char* WIFI_PASS = "Write your wifi password here";
const char* TB_SERVER = "thingsboard.cloud"; // Change if using a different ThingsBoard instance
const uint16_t TB_PORT = 1883;
const char* TB_TOKEN  = "Write your ThingsBoard token here";

// --- PINS ---
#define RELAY_PIN 4      // Active LOW/HIGH depending on module (Adjust in controlPump)
#define LORA_SS   5
#define LORA_RST  14
#define LORA_DIO0 26

// --- DFPLAYER PINS (Serial2) ---
#define FPSerial Serial2 
// ESP32 GPIO 16 (RX) <-> DFPlayer TX
// ESP32 GPIO 17 (TX) <-> 1k Resistor <-> DFPlayer RX

// --- OBJECTS ---
WiFiClient espClient;
Arduino_MQTT_Client mqttClient(espClient);
DFRobotDFPlayerMini myDFPlayer;

// --- STATE VARIABLES ---
bool autoMode      = true;       
bool pumpState     = false;      
int  waterLevel    = 0;          
bool loraConnected = false;      
unsigned long lastLoRaTime = 0;
const unsigned long LORA_TIMEOUT = 300000UL;  // 5 mins

bool wifiOk = false;
bool mqttOk = false;
unsigned long lastMqttRetry = 0;
const unsigned long MQTT_RETRY_INTERVAL = 10000UL; 

// --- AUDIO TRACK MAPPING ---
// 1:SystemReady, 2:PumpOn, 3:PumpOff, 4:LowLevel, 5:TankFull, 6:WifiOk, 7:WifiLost, 8:LoRaLost

// --- FORWARD DECLARATIONS ---
void controlPump(bool state);
void mqttDataCallback(char const * topic, uint8_t const * payload, size_t length);
void handleRpc(const char* topic, const uint8_t* payload, size_t len);
void sendTelemetry(const JsonDocument& doc);
void ensureWifiAndMqttNonBlocking();
void playAudio(int trackNum);

void setup() {
  Serial.begin(115200);

  // Relay Setup
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Assume LOW = OFF initially

  Serial.println("=== SYSTEM STARTING ===");

  // --- AUDIO SETUP ---
  FPSerial.begin(9600, SERIAL_8N1, 16, 17);
  Serial.print("Initializing DFPlayer... ");
  if (myDFPlayer.begin(FPSerial)) {
    Serial.println("OK!");
    myDFPlayer.volume(25);  // Set volume (0-30)
    playAudio(1);           // "System Ready"
  } else {
    Serial.println("Failed! Check connections.");
  }

  // --- WIFI SETUP ---
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) {
    delay(500);
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiOk = true;
    Serial.println("WiFi Connected");
    playAudio(6); // "WiFi Connected"
  } else {
    wifiOk = false;
    Serial.println("Offline Mode Active");
    playAudio(7); // "Offline Mode"
  }

  // --- LORA SETUP ---
  SPI.begin(18, 19, 23, 5);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa Init Failed!");
    while (true);
  }
  LoRa.setSyncWord(0xF3);
  Serial.println("LoRa OK");

  // --- MQTT SETUP ---
  mqttClient.set_server(TB_SERVER, TB_PORT);
  mqttClient.set_data_callback(mqttDataCallback);
}

void loop() {
  // 1. Connection Maintenance
  ensureWifiAndMqttNonBlocking();

  // 2. LoRa Receiver
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String packet = "";
    while (LoRa.available()) packet += (char)LoRa.read();

    if (packet.startsWith("TANK1:")) {
      waterLevel = packet.substring(6).toInt();
      lastLoRaTime = millis();
      loraConnected = true;

      Serial.print("Level: "); Serial.println(waterLevel);

      if (mqttOk) {
        StaticJsonDocument<128> doc;
        doc["waterLevel"] = waterLevel;
        doc["loraStatus"] = true;
        sendTelemetry(doc);
      }
    }
  }

  // 3. Watchdog
  if (loraConnected && (millis() - lastLoRaTime > LORA_TIMEOUT)) {
    Serial.println("ALERT: LoRa Lost");
    loraConnected = false;
    playAudio(8); // "Signal Lost"
    controlPump(false); 
    
    if (mqttOk) {
      StaticJsonDocument<64> doc;
      doc["loraStatus"] = false;
      sendTelemetry(doc);
    }
  }

  // 4. Logic
  if (autoMode && loraConnected) {
    if (waterLevel < 20 && !pumpState) {
      playAudio(4); // "Low Level"
      delay(2000);  // Wait for speech to finish before pump noise
      controlPump(true);
    }
    if (waterLevel >= 100 && pumpState) {
      playAudio(5); // "Tank Full"
      delay(2000);
      controlPump(false);
    }
  }

  if (mqttOk) mqttClient.loop();
}

// --- HELPER FUNCTIONS ---

void playAudio(int trackNum) {
  // Simple non-blocking check could be added here, but direct play is usually fine
  myDFPlayer.play(trackNum);
}

void ensureWifiAndMqttNonBlocking() {
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiOk) {
      wifiOk = false;
      mqttOk = false;
      autoMode = true; 
      playAudio(7); // "Connection Lost"
    }
    // Reconnect logic...
    static unsigned long lastWifiRetry = 0;
    if (millis() - lastWifiRetry > MQTT_RETRY_INTERVAL) {
      lastWifiRetry = millis();
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASS);
    }
    return;
  } 
  
  if (!wifiOk) {
    wifiOk = true;
    playAudio(6); // "WiFi Connected"
  }

  if (!mqttClient.connected()) {
    mqttOk = false;
    autoMode = true;
    if (millis() - lastMqttRetry > MQTT_RETRY_INTERVAL) {
      lastMqttRetry = millis();
      if (mqttClient.connect("ESP32-PumpCtrl", TB_TOKEN, "")) {
        mqttClient.subscribe("v1/devices/me/rpc/request/+");
        mqttOk = true;
      }
    }
  } else {
    mqttOk = true;
  }
}

void controlPump(bool state) {
  // Prevent redundant plays if state isn't changing
  if (pumpState != state) { 
    if (state) playAudio(2); // "Pump Started"
    else playAudio(3);       // "Pump Stopped"
  }

  pumpState = state;
  digitalWrite(RELAY_PIN, state ? HIGH : LOW); // Check your relay logic!

  if (mqttOk) {
    StaticJsonDocument<64> doc;
    doc["pumpState"] = pumpState;
    doc["autoMode"] = autoMode;
    sendTelemetry(doc);
  }
}

void sendTelemetry(const JsonDocument& doc) {
  if (!mqttOk) return;
  char buf[256];
  size_t n = serializeJson(doc, buf);
  mqttClient.publish("v1/devices/me/telemetry", (uint8_t*)buf, n);
}

void mqttDataCallback(char const * topic, uint8_t const * payload, size_t length) {
  handleRpc(topic, payload, length);
}

void handleRpc(const char* topic, const uint8_t* payload, size_t len) {
  if (!mqttOk) return;
  String t(topic);
  if (!t.startsWith("v1/devices/me/rpc/request/")) return;

  StaticJsonDocument<256> doc;
  deserializeJson(doc, payload, len);
  const char* method = doc["method"];
  JsonVariant params = doc["params"];

  String respTopic = t;
  respTopic.replace("request", "response");
  StaticJsonDocument<128> respDoc;

  if (strcmp(method, "setMode") == 0) {
    autoMode = params.as<bool>();
    respDoc["autoMode"] = autoMode;
    // Optional: playAudio(9) -> "Manual Mode"
  }
  else if (strcmp(method, "setPump") == 0) {
    if (!autoMode) {
      controlPump(params.as<bool>());
    }
    respDoc["pumpState"] = pumpState;
  }
bgb
  char buf[128];
  size_t n = serializeJson(respDoc, buf);
  mqttClient.publish(respTopic.c_str(), (uint8_t*)buf, n);
}
