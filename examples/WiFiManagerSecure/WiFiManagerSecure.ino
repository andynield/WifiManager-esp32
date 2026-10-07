/*
  WiFiManager Custom - Secure Portal Example
  Starts a configuration portal protected by an AP password.
*/

#include <WiFiManagerCustom.h>

WiFiManagerCustom wifiManager;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Configure to require AP password (pass second param to autoConnect/startConfigPortal)
  const char* apPassword = "secret123";
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setDebugOutput(true);
  
  // This will start the portal with a password when needed
  if (!wifiManager.autoConnect("SecureDevice", apPassword)) {
    Serial.println("Portal active with password 'secret123'");
  }
}

void loop() {
  if (wifiManager.portalRunning()) {
    wifiManager.process();
  }
  delay(10);
}
