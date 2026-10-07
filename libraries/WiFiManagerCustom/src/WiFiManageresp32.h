/*
   Custom WiFi Manager for ESP32
   Replacement for tzapu/WiFiManager
   Designed for ESP32-C3 and compatible boards
   
   Features:
   - WiFi scanning and connection
   - Configuration portal (captive portal)
   - Persistent credential storage
   - Non-blocking mode
   - Web-based configuration interface
*/

#ifndef WIFI_MANAGER_CUSTOM_H
#define WIFI_MANAGER_CUSTOM_H

#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <functional>

typedef std::function<void()> ConfigCallback;

class WiFiManageresp32 {
public:
    WiFiManageresp32();
    WiFiManageresp32();

    // Main functions
    bool autoConnect(const char* apName);
    bool autoConnect(const char* apName, const char* apPassword);
    bool startConfigPortal(const char* apName);
    bool startConfigPortal(const char* apName, const char* apPassword);
    
    // Callback
    void setSaveConfigCallback(ConfigCallback callback);
    
    // Configuration
    void setHostname(const char* hostname);
    void setConfigPortalBlocking(bool blocking);
    void setDebugOutput(bool debug);
    void setConnectTimeout(uint16_t seconds);
    void setConfigPortalTimeout(uint16_t seconds);
    
    // WiFi management
    void disconnect();
    String getSSID();
    String getPassword();
    bool isConnected();
    std::vector<String> scanNetworks();
    int getRSSI(const char* ssid = nullptr);
    
    // Portal control
    void process();
    bool portalRunning();
    void stopConfigPortal();
    
    // Network information
    String getIP();
    String getMacAddress();
    int getSignalStrength();

private:
    // Configuration storage
    struct WiFiConfig {
        char ssid[33];
        char password[65];
        char hostname[33];
    } config;
    
    // State management
    bool portalActive;
    bool blockingMode;
    bool debugOutput;
    bool configSaved;
    unsigned long connectStartTime;
    unsigned long configPortalStartTime;
    
    // Timeouts (in seconds)
    uint16_t connectTimeout;
    uint16_t configPortalTimeout;
    
    // Web server
    WebServer* server;
    DNSServer* dnsServer;
    
    // Callbacks
    ConfigCallback onSaveConfigCallback;
    
    // Helper functions
    bool loadConfig();
    bool saveConfig();
    void setupConfigPortal(const char* apName, const char* apPassword);
    void handleConfigRequest();
    void handleConfigSave();
    void handleScanNetworks();
    void handleStatus();
    void handleNotFound();
    void handleRoot();
    void startAccessPoint(const char* apName, const char* apPassword);
    void stopAccessPoint();
    bool connectToNetwork();
    void setupDNS();
    void cleanupConfigPortal();
    String generateConfigHTML();
    String generateNetworkJSON(const std::vector<String>& networks);
};

#endif // WIFI_MANAGER_CUSTOM_H
