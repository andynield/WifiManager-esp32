/*
  WiFiManager Custom - OTA Example
  Demonstrates integration with ArduinoOTA after WiFi is configured.
*/

#include <WiFiManagerCustom.h>
#include <ArduinoOTA.h>

WiFiManagerCustom wifiManager;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setDebugOutput(true);
  wifiManager.autoConnect("ESP32-OTA");

  if (wifiManager.isConnected()) {
    // Configure ArduinoOTA
    ArduinoOTA.onStart([]() {
      Serial.println("OTA Start");
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("OTA End");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("OTA Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

    ArduinoOTA.begin();
    Serial.println("OTA Ready");
  }
}

void loop() {
  if (wifiManager.portalRunning()) {
    wifiManager.process();
  }
  if (wifiManager.isConnected()) {
    ArduinoOTA.handle();
  }
  delay(10);
}
