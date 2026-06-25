# Automatic Medicine Dispenser (ESP32 + Telegram)

ESP32 + Servo + Buzzer + 3 Buttons + I2C LCD + Telegram Notification.

## Features
- 3 dose times per day (set via buttons)
- NTP internet time (auto-sync via WiFi)
- Servo dispenses at scheduled time
- Buzzer alerts when medicine time
- Telegram push notification to your phone
- Settings saved in ESP32 memory (survives power loss)
- Auto resets daily at midnight

## Components
- ESP32 Dev Board
- 1x SG90 Servo Motor
- 1x 16x2 I2C LCD
- 1x Active Buzzer (5V)
- 3x Tactile Push Buttons
- USB cable / power bank

## Connections

| Component | ESP32 Pin |
|-----------|-----------|
| Servo Signal | GPIO 19 |
| Servo VCC | 5V (VIN) |
| Servo GND | GND |
| Buzzer (+) | GPIO 18 |
| Buzzer (-) | GND |
| Button 1 (SET) | GPIO 25 |
| Button 2 (UP) | GPIO 26 |
| Button 3 (DOWN) | GPIO 27 |
| LCD SDA | GPIO 21 |
| LCD SCL | GPIO 22 |
| LCD VCC | 3.3V |
| LCD GND | GND |

## Button Functions
- **SET (25):** Enter setting mode → cycle through D1 Hour → D1 Min → D2 Hour → D2 Min → D3 Hour → D3 Min → Save
- **UP (26):** +1 to current setting field
- **DOWN (27):** -1 to current setting field

## Default Dose Times
- Dose 1: 08:00
- Dose 2: 12:25
- Dose 3: 20:00

## Telegram Setup
1. Open Telegram → search @BotFather
2. Send `/newbot` → choose name → get Token
3. Message your bot → visit `https://api.telegram.org/bot<TOKEN>/getUpdates`
4. Copy Chat ID from response

## Libraries Needed
- ESP32Servo
- LiquidCrystal_I2C
- Preferences (built-in)

## Power
- Power the ESP32 via USB (5V)
- Servo gets 5V from ESP32 VIN pin
- Common GND between all components
