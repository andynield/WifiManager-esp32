# WiFiManager-Standalone

Standalone Arduino library and example for the custom WiFi Manager (ESP32 / ESP32-C3).

## Structure

- libraries/WiFiManagerCustom/src - Library source (header, cpp, config)
- examples/WiFiManagerTest - Arduino example sketch (open in Arduino IDE)

## Installation

Option A (Arduino IDE):
1. Copy the `libraries/WiFiManagerCustom` folder into your sketchbook `libraries/` folder
2. Restart Arduino IDE
3. Open `File > Examples > WiFiManagerCustom > WiFiManagerTest` or open `examples/WiFiManagerTest/WiFiManagerTest.ino` directly

Option B (PlatformIO):
1. Add this folder to your PlatformIO workspace and include the library by path, or install the library via `lib/deps` when published.

## Example
Open `examples/WiFiManagerTest/WiFiManagerTest.ino` and upload to your ESP32-C3 board. The device will either connect using stored credentials or start an AP and configuration portal at `http://192.168.4.1`.
