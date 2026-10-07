/*
   WiFiManager Custom - Arduino Example
   Open this sketch from Arduino IDE: File -> Examples -> WiFiManagerCustom -> WiFiManagerTest
*/

#include <WiFiManagerCustom.h>
#include <WebServer.h>
#include <Preferences.h>

WiFiManagerCustom wifiManager;
bool portalActive = false;
WebServer testServer(8080);

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n\nCustom WiFi Manager - Example");

    wifiManager.setHostname("ESP32-C3-Test");
    wifiManager.setConfigPortalBlocking(false);
    wifiManager.setDebugOutput(true);
    wifiManager.setConnectTimeout(20);
    wifiManager.setConfigPortalTimeout(180);

    wifiManager.setSaveConfigCallback([]() {
        Serial.println("\n[CALLBACK] WiFi configuration saved!");
    });

    String deviceName = String("ESP32C3-") + String(ESP.getEfuseMac(), HEX);
    if (!wifiManager.autoConnect(deviceName.c_str())) {
        Serial.println("Auto-connect returned false, portal may be active");
    }

    if (wifiManager.portalRunning()) {
        portalActive = true;
        Serial.println("Configuration portal active. Connect to AP and open http://192.168.4.1");
    } else if (wifiManager.isConnected()) {
        Serial.println("WiFi connected: ");
        Serial.println(wifiManager.getIP());
        setupTestServer();
    }
}

void loop() {
    if (portalActive) {
        wifiManager.process();
        if (!wifiManager.portalRunning()) {
            portalActive = false;
            if (wifiManager.isConnected()) {
                Serial.println("WiFi connected after portal!");
                setupTestServer();
            }
        }
    }

    if (wifiManager.isConnected()) {
        testServer.handleClient();
    }

    delay(10);
}

void setupTestServer() {
    static bool serverStarted = false;
    if (serverStarted) return;

    testServer.on("/", HTTP_GET, []() {
        String html = "<html><body><h1>WiFi Test</h1>";
        html += "<p>SSID: " + wifiManager.getSSID() + "</p>";
        html += "<p>IP: " + wifiManager.getIP() + "</p>";
        html += "<p>Signal: " + String(wifiManager.getSignalStrength()) + " dBm</p>";
        html += "</body></html>";
        testServer.send(200, "text/html", html);
    });

    testServer.on("/scan", HTTP_GET, []() {
        std::vector<String> nets = wifiManager.scanNetworks();
        String res = "<html><body><h1>Networks</h1><ul>";
        for (auto &n : nets) res += "<li>" + n + "</li>";
        res += "</ul></body></html>";
        testServer.send(200, "text/html", res);
    });

    testServer.begin();
    Serial.println("Test server started on port 8080");
    serverStarted = true;
}
