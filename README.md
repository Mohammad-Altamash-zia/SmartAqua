# SmartAqua: Long-Range IoT Water Management System 💧

SmartAqua is a robust, end-to-end IoT solution designed to automate residential water management. It addresses critical inefficiencies such as water wastage due to overflow and the unreliability of standard WiFi signals in multi-story concrete structures.

By utilizing **LoRa (Long Range)** radio technology, the system achieves stable communication over 1km+ without internet reliance, while integrating an **ESP32-based Cloud Gateway** for global remote monitoring via ThingsBoard.

---

## 🚀 Key Features

* **Long Range (LoRa):** Communicates through 4+ concrete slabs and up to 1km in urban environments.
* **Voice Feedback:** Natural language alerts (e.g., "Pump Started", "Tank Full") via DFPlayer Mini & PAM8403.
* **Cloud Integration:** Real-time monitoring and manual override via **ThingsBoard Cloud**.
* **Power Resilience:** Built-in "Mini-UPS" with 10,800mAh battery capacity to survive 48-hour power cuts.
* **Adaptive Transmission:** Smart sleep logic based on water levels to extend battery life.
* **Rugged Engineering:** Wildlife-protected (monkey-proof) masonry housing for the roof-top unit.

---

## 🏗️ System Architecture

The project consists of two specialized nodes:

### 1. The Sender (Roof Node)
* **Hardware:** Arduino Nano, LoRa SX1278, HC-SR04 Ultrasonic Sensor.
* **Innovation:** Features **"Sensor Isolation"** where the sensor is only powered during measurement to save current. It uses **"Burst Fire"** transmission logic (sending data 3 times) to guarantee packet delivery.

### 2. The Receiver (Room Node)
* **Hardware:** ESP32 Dev Module, DFPlayer Mini, PAM8403 Amplifier, 3W Speaker, 5V Relay.
* **Innovation:** Acts as a gateway between LoRa and WiFi/MQTT. Includes a safety **Watchdog Timer** that automatically shuts down the pump if the signal from the roof is lost.

---

## 🛠️ Hardware Setup & Wiring

### Unit A: Sender (Arduino Nano)

| Component | Pin (MCU/Other) | Function |
| :--- | :--- | :--- |
| LoRa NSS | D10 (via LLC) | SPI Chip Select |
| Sensor VCC | **D5** | Dynamic Power Control |
| Trig / Echo | D3 / D4 | Measurement |
| Power | VIN | UPS Main Supply |

### Unit B: Receiver (ESP32)

| Component | Pin (MCU/Other) | Function |
| :--- | :--- | :--- |
| LoRa NSS | GPIO 5 | SPI Chip Select |
| Relay IN | GPIO 4 | Pump Switching |
| DFPlayer RX | GPIO 17 (1kΩ Res) | Audio Control |
| Amp Filter | 100uF Cap | Noise Mitigation |

---

## 🧱 Real-World Challenges & Solutions

### 🐒 Monkey-Proofing & Weatherproofing
Roof-top electronics face extreme heat and interference from local wildlife. To protect the Sender Unit:
1. **Enclosure:** IP65-rated waterproof box sealed with hot glue.
2. **Masonry Shield:** Built a protective structure using **stone bricks and cement** to prevent monkeys from pulling wires and to provide thermal insulation for the Li-ion batteries.

### 🔇 Audio Noise Suppression
Initial tests showed a "hissing" sound in the speaker. This was solved by:
* Adding a **1kΩ resistor** on the serial communication line between ESP32 and DFPlayer.
* Adding a **100uF capacitor** across the PAM8403 power rail to filter out WiFi signal interference.

---

## 💻 Installation & Usage

1. **Library Requirements:** Install `LoRa`, `LowPower`, `DFRobotDFPlayerMini`, and `ArduinoJson`.
2. **ThingsBoard Setup:** Create a new device on ThingsBoard Cloud, copy the Access Token, and paste it into the ESP32 code.
3. **Flash Code:** Upload the Sender code to the Nano and the Receiver code to the ESP32.
4. **Audio Files:** Format a microSD card as FAT32, create an `mp3` folder, and add files `0001.mp3` to `0008.mp3`.

---

## 📜 License
This project is licensed under the MIT License.

## 🤝 Contact
**Mohammad Altamash Zia** | [LinkedIn](https://www.linkedin.com/in/mohammad-altamash-zia-398a9a301/) | [Email](mailto:zmohammadaltamash@gmail.com)
