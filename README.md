# 🐝 IoT Bee Weight Scale System

A smart IoT system for real-time monitoring of beehive conditions.  
The project enables continuous measurement of hive weight, temperature, and humidity, local data display on an LCD screen, and cloud-based data storage and visualization using Firebase and a web dashboard.

---

## 🎯 Project Objective

The main goal of this project is to develop a **reliable IoT monitoring system** that:
- continuously measures beehive weight
- monitors basic microclimate conditions
- detects sudden changes (e.g. weight drop due to swarming or theft)
- enables remote monitoring via a web application

---

## 🧠 System Architecture

The system consists of three main components:

1. **IoT Device (ESP8266)**
2. **Cloud Service (Firebase Realtime Database)**
3. **Web Application (Dashboard)**

Workflow:
- ESP8266 collects sensor data
- Data is sent to Firebase in real time
- The web dashboard visualizes data for the user

---

## 🔌 Hardware Components

- ESP8266 (NodeMCU)
- HX711 Load Cell Amplifier
- Load Cell (20 kg)
- DHT11 / DHT22 (Temperature & Humidity)
- DS3231 RTC Module
- I2C LCD 16x2
- Buzzer (SOS alarm)
- USB Power Supply

---

## 💻 Software Technologies

### Firmware
- Arduino (C++)
- ESP8266WiFi
- Firebase ESP Client
- HX711 Library
- RTClib
- DHT Sensor Library

### Cloud / Backend
- Firebase Realtime Database
- Firebase Authentication (Email/Password)

### Frontend
- Angular
- TypeScript
- Bootstrap
- Real-time Firebase data synchronization

---

## 📟 Features

- 📊 Beehive weight measurement
- 🌡️ Temperature and humidity monitoring
- ⏰ Accurate timestamp using RTC
- 🔔 SOS alarm (3 short – 3 long – 3 short)
- 🖥️ Local LCD data display
- ☁️ Cloud data storage (Firebase)
- 📈 Web dashboard with dynamic status (Normal / Alert / Decrease)

---

## 🚨 Anomaly Detection

The system automatically detects:
- sudden weight drops
- abnormal temperature or humidity values
- WiFi or Firebase connection issues

When an anomaly is detected:
- the buzzer activates an SOS signal
- the dashboard status changes to **Alert**

---

## 🔄 State Machine (Device Logic)

- Boot
- Calibration
- WiFi scanning
- Access Point mode (if no known networks are found)
- Firebase connection
- Idle state
- Periodic sensor readings
- Data transmission and LCD update

---

## 📦 Installation & Setup

### 1. Firmware Setup
1. Open the project in Arduino IDE
2. Install all required libraries
3. Configure WiFi and Firebase credentials
4. Upload firmware to ESP8266

### 2. Firebase Setup
- Create a Firebase Realtime Database
- Enable Email/Password authentication
- Configure database access rules

### 3. Frontend Setup
```bash
npm install
ng serve
