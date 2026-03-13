/*
 * UNIT A: ROOF TANK SENDER (Deep Sleep + Sensor Power Control)
 * BATTERY LIFE: Optimized (Sensor OFF during sleep)
 * WIRING: Connect Ultrasonic VCC to Arduino Pin D5
 * REQUIRES: 'LowPower' library by Rocket Scream
 */

#include <SPI.h>
#include <LoRa.h>
#include "LowPower.h" 

// --- PINS ---
#define TRIG_PIN  3
#define ECHO_PIN  4
#define SENSOR_POWER_PIN 5  // NEW: Powers the sensor (VCC)
#define LORA_SS   10
#define LORA_RST  9
#define LORA_DIO0 2

// --- CONFIG ---
const int DISTANCE_EMPTY = 129; 
const int DISTANCE_FULL  = 19;

void setup() {
  // Serial.begin(9600); // Commented out to save power
  
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  // Configure the Power Pin for the Sensor
  pinMode(SENSOR_POWER_PIN, OUTPUT);
  digitalWrite(SENSOR_POWER_PIN, LOW); // Start with sensor OFF

  // Initialize LoRa
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    // Error indicator: Fast blink forever
    pinMode(LED_BUILTIN, OUTPUT);
    while(1) {
      digitalWrite(LED_BUILTIN, HIGH); delay(100);
      digitalWrite(LED_BUILTIN, LOW); delay(100);
    }
  }
  LoRa.setSyncWord(0xF3);
  LoRa.setTxPower(17); 
  
  // Put LoRa to sleep immediately
  LoRa.sleep();
}

void loop() {
  // 1. WAKE UP SENSOR
  digitalWrite(SENSOR_POWER_PIN, HIGH); // Turn ON VCC to sensor
  delay(100); // Wait 100ms for sensor to stabilize (Crucial!)

  // 2. MEASURE
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout
  int distance = duration * 0.034 / 2;

  // 3. POWER OFF SENSOR
  digitalWrite(SENSOR_POWER_PIN, LOW); // Cut power immediately to save 15mA

  // Map and constrain values
  int waterLevel = map(distance, DISTANCE_EMPTY, DISTANCE_FULL, 0, 100);
  waterLevel = constrain(waterLevel, 0, 100);

  // 4. WAKE UP LORA & SEND
  // LoRa wakes automatically on beginPacket()
  LoRa.beginPacket();
  LoRa.print("TANK1:");
  LoRa.print(waterLevel);
  LoRa.endPacket();
  
  // 5. PUT LORA TO SLEEP
  LoRa.sleep(); 
  
  // 6. DEEP SLEEP LOGIC
  // If tank is full (>85%), sleep shortly (8 sec)
  // If tank is safe (<85%), sleep long (2 mins)
  
  if (waterLevel >= 85) {
     // DANGER ZONE: Sleep 8 seconds
     LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  } 
  else {
     // SAFE ZONE: Sleep 2 Minutes (15 cycles of 8s)
     for(int i=0; i<15; i++) {
       LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
     }
  }
}
