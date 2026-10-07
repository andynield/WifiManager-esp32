/*
   Custom WiFi Manager Implementation for ESP32
   Replacement for tzapu/WiFiManager
*/

#include "WiFiManageresp32.h"
#include <Preferences.h>

static const char* STORAGE_KEY = "wifi_config";
static const char* STORAGE_NAMESPACE = "WiFiManageresp32";
static const int DNS_PORT = 53;
static const int CONFIG_SERVER_PORT = 80;

WiFiManageresp32::WiFiManageresp32() 
    : portalActive(false), blockingMode(true), debugOutput(false),
      configSaved(false), connectTimeout(20), configPortalTimeout(180),
      server(nullptr), dnsServer(nullptr) {
    memset(&config, 0, sizeof(config));
}

WiFiManageresp32::~WiFiManageresp32() {
    cleanupConfigPortal();
}

bool WiFiManageresp32::autoConnect(const char* apName) {
    return autoConnect(apName, nullptr);
}

bool WiFiManageresp32::autoConnect(const char* apName, const char* apPassword) {
    // Try to load saved configuration
    if (loadConfig() && connectToNetwork()) {
        if (debugOutput) {
            Serial.println("[WiFiManager] Connected using saved credentials");
        }
        return true;
    }
    
    // If connection fails or no saved config, start config portal
    if (debugOutput) {
        Serial.println("[WiFiManager] Failed to connect, starting config portal");
    }
    
    return startConfigPortal(apName, apPassword);
}

bool WiFiManageresp32::startConfigPortal(const char* apName) {
    return startConfigPortal(apName, nullptr);
}

bool WiFiManageresp32::startConfigPortal(const char* apName, const char* apPassword) {
    setupConfigPortal(apName, apPassword);
    
    if (blockingMode) {
        // Blocking mode - wait until config is done
        configPortalStartTime = millis();
        
        while (portalActive) {
            process();
            delay(10);
            
            // Check timeout
            if (configPortalTimeout > 0 && 
                (millis() - configPortalStartTime) / 1000 >= configPortalTimeout) {
                if (debugOutput) {
                    Serial.println("[WiFiManager] Config portal timeout");
                }
                break;
            }
        }
    }
    
    return configSaved;
}

void WiFiManageresp32::setupConfigPortal(const char* apName, const char* apPassword) {
    if (debugOutput) {
        Serial.printf("[WiFiManager] Starting config portal: %s\n", apName);
    }
    
    // Start access point
    startAccessPoint(apName, apPassword);
    
    // Initialize server
    if (server == nullptr) {
        server = new WebServer(CONFIG_SERVER_PORT);
    }
    
    // Setup routes
    server->on("/", HTTP_GET, [this]() { handleRoot(); });
    server->on("/config", HTTP_GET, [this]() { handleConfigRequest(); });
    server->on("/save", HTTP_POST, [this]() { handleConfigSave(); });
    server->on("/scan", HTTP_GET, [this]() { handleScanNetworks(); });
    server->on("/status", HTTP_GET, [this]() { handleStatus(); });
    server->onNotFound([this]() { handleNotFound(); });
    
    server->begin();
    
    // Setup DNS
    setupDNS();
    
    portalActive = true;
    configSaved = false;
}

void WiFiManageresp32::setupDNS() {
    if (dnsServer == nullptr) {
        dnsServer = new DNSServer();
    }
    dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer->start(DNS_PORT, "*", WiFi.softAPIP());
}

void WiFiManageresp32::process() {
    if (portalActive) {
        if (dnsServer) {
            dnsServer->processNextRequest();
        }
        if (server) {
            server->handleClient();
        }
    }
}

void WiFiManageresp32::handleRoot() {
    server->send(200, "text/html", generateConfigHTML());
}

void WiFiManageresp32::handleConfigRequest() {
    // Return current configuration
    StaticJsonDocument<512> doc;
    doc["ssid"] = config.ssid;
    doc["hostname"] = config.hostname;
    
    String response;
    serializeJson(doc, response);
    
    server->send(200, "application/json", response);
}

void WiFiManageresp32::handleConfigSave() {
    if (server->hasArg("ssid") && server->hasArg("password")) {
        strncpy(config.ssid, server->arg("ssid").c_str(), sizeof(config.ssid) - 1);
        strncpy(config.password, server->arg("password").c_str(), sizeof(config.password) - 1);
        
        if (server->hasArg("hostname")) {
            strncpy(config.hostname, server->arg("hostname").c_str(), sizeof(config.hostname) - 1);
        }
        
        if (saveConfig()) {
            if (debugOutput) {
                Serial.println("[WiFiManager] Configuration saved");
            }
            configSaved = true;
            portalActive = false;
            
            server->send(200, "application/json", "{\"status\":\"saved\"}");
            
            // Call callback if registered
            if (onSaveConfigCallback) {
                onSaveConfigCallback();
            }
            
            // Cleanup and attempt connection
            cleanupConfigPortal();
            if (connectToNetwork()) {
                if (debugOutput) {
                    Serial.println("[WiFiManager] Connected to network");
                }
            }
        } else {
            server->send(500, "application/json", "{\"status\":\"error\"}");
        }
    } else {
        server->send(400, "application/json", "{\"status\":\"missing_params\"}");
    }
}

void WiFiManageresp32::handleScanNetworks() {
    std::vector<String> networks = scanNetworks();
    String response = generateNetworkJSON(networks);
    server->send(200, "application/json", response);
}

void WiFiManageresp32::handleStatus() {
    StaticJsonDocument<256> doc;
    doc["connected"] = WiFi.status() == WL_CONNECTED;
    doc["ssid"] = WiFi.SSID();
    doc["ip"] = WiFi.localIP().toString();
    doc["signal"] = WiFi.RSSI();
    
    String response;
    serializeJson(doc, response);
    
    server->send(200, "application/json", response);
}

void WiFiManageresp32::handleNotFound() {
    // Redirect to root
    server->sendHeader("Location", "/", true);
    server->send(302, "text/plain", "");
}

void WiFiManageresp32::startAccessPoint(const char* apName, const char* apPassword) {
    WiFi.mode(WIFI_AP_STA);
    
    if (apPassword && strlen(apPassword) > 0) {
        WiFi.softAP(apName, apPassword);
    } else {
        WiFi.softAP(apName);
    }
    
    if (debugOutput) {
        Serial.printf("[WiFiManager] AP started: %s, IP: %s\n", apName, WiFi.softAPIP().toString().c_str());
    }
}

void WiFiManageresp32::stopAccessPoint() {
    WiFi.mode(WIFI_STA);
    WiFi.softAPdisconnect(true);
}

bool WiFiManageresp32::connectToNetwork() {
    if (strlen(config.ssid) == 0) {
        return false;
    }
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.ssid, config.password);
    
    connectStartTime = millis();
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        if (debugOutput) {
            Serial.print(".");
        }
        
        // Check timeout
        if ((millis() - connectStartTime) / 1000 >= connectTimeout) {
            if (debugOutput) {
                Serial.println("\n[WiFiManager] Connection timeout");
            }
            return false;
        }
    }
    
    if (debugOutput) {
        Serial.printf("\n[WiFiManager] Connected to %s\n", config.ssid);
        Serial.printf("[WiFiManager] IP: %s\n", WiFi.localIP().toString().c_str());
    }
    
    return true;
}

std::vector<String> WiFiManageresp32::scanNetworks() {
    std::vector<String> networks;
    
    int n = WiFi.scanNetworks();
    
    for (int i = 0; i < n; i++) {
        networks.push_back(WiFi.SSID(i));
    }
    
    return networks;
}

bool WiFiManageresp32::loadConfig() {
    Preferences preferences;
    preferences.begin(STORAGE_NAMESPACE);
    
    String ssid = preferences.getString("ssid", "");
    String password = preferences.getString("password", "");
    String hostname = preferences.getString("hostname", "ESP32");
    
    preferences.end();
    
    if (ssid.length() > 0) {
        strncpy(config.ssid, ssid.c_str(), sizeof(config.ssid) - 1);
        strncpy(config.password, password.c_str(), sizeof(config.password) - 1);
        // Fixed typo: hostname field
        strncpy(config.hostname, hostname.c_str(), sizeof(config.hostname) - 1);
        return true;
    }
    return false;
}

bool WiFiManageresp32::saveConfig() {
    Preferences preferences;
    preferences.begin(STORAGE_NAMESPACE, false);

    preferences.putString("ssid", String(config.ssid));
    preferences.putString("password", String(config.password));
    preferences.putString("hostname", String(config.hostname));

    preferences.end();

    if (debugOutput) {
        Serial.println("[WiFiManager] Saved configuration to Preferences");
    }

    return true;
}

void WiFiManageresp32::cleanupConfigPortal() {
    // Stop web server
    if (server) {
        server->stop();
        delete server;
        server = nullptr;
    }

    // Stop DNS server
    if (dnsServer) {
        dnsServer->stop();
        delete dnsServer;
        dnsServer = nullptr;
    }

    // Stop AP
    stopAccessPoint();

    portalActive = false;

    if (debugOutput) {
        Serial.println("[WiFiManager] Cleaned up config portal");
    }
}

void WiFiManageresp32::setSaveConfigCallback(ConfigCallback callback) {
    onSaveConfigCallback = callback;
}

void WiFiManageresp32::setHostname(const char* hostname) {
    if (hostname && strlen(hostname) > 0) {
        strncpy(config.hostname, hostname, sizeof(config.hostname) - 1);
        WiFi.setHostname(config.hostname);
    }
}

void WiFiManageresp32::setConfigPortalBlocking(bool blocking) {
    blockingMode = blocking;
}

void WiFiManageresp32::setDebugOutput(bool debug) {
    debugOutput = debug;
}

void WiFiManageresp32::setConnectTimeout(uint16_t seconds) {
    connectTimeout = seconds;
}

void WiFiManageresp32::setConfigPortalTimeout(uint16_t seconds) {
    configPortalTimeout = seconds;
}

void WiFiManageresp32::disconnect() {
    WiFi.disconnect(true, true);
}

String WiFiManageresp32::getSSID() {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.SSID();
    }
    return String(config.ssid);
}

String WiFiManageresp32::getPassword() {
    return String(config.password);
}

bool WiFiManageresp32::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

int WiFiManageresp32::getRSSI(const char* ssid) {
    if (ssid == nullptr) {
        return WiFi.RSSI();
    }

    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; ++i) {
        if (WiFi.SSID(i) == String(ssid)) {
            return WiFi.RSSI(i);
        }
    }
    return 0;
}

bool WiFiManageresp32::portalRunning() {
    return portalActive;
}

void WiFiManageresp32::stopConfigPortal() {
    portalActive = false;
    cleanupConfigPortal();
}

String WiFiManageresp32::getIP() {
    return WiFi.localIP().toString();
}

String WiFiManageresp32::getMacAddress() {
    return WiFi.macAddress();
}

int WiFiManageresp32::getSignalStrength() {
    return WiFi.RSSI();
}

String WiFiManageresp32::generateConfigHTML() {
    String page = "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">";
    page += "<title>Configure WiFi</title></head><body>";
    page += "<h2>WiFi Configuration</h2>";
    page += "<form method='POST' action='/save'>";

    page += "SSID:<br><input type='text' name='ssid' value='" + String(config.ssid) + "'><br>";
    page += "Password:<br><input type='password' name='password' value='" + String(config.password) + "'><br>";
    page += "Hostname:<br><input type='text' name='hostname' value='" + String(config.hostname) + "'><br><br>";
    page += "<input type='submit' value='Save'>";
    page += "</form>";

    page += "<h3>Nearby Networks</h3>";
    page += "<button onclick=\"fetch('/scan').then(r=>r.json()).then(j=>{let o=document.getElementById('nets'); o.innerHTML=''; j.forEach(n=>{ o.innerHTML += '<div>'+n.ssid+' ('+n.rssi+' dBm)'+(n.secured? ' 🔒':'')+'</div>'; });})\">Scan</button>";
    page += "<div id='nets'></div>";

    page += "</body></html>";
    return page;
}

String WiFiManageresp32::generateNetworkJSON(const std::vector<String>& networks) {
    // Build array by performing a fresh scan to include RSSI and encryption info
    StaticJsonDocument<1024> doc;
    JsonArray arr = doc.to<JsonArray>();

    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; ++i) {
        JsonObject obj = arr.createNestedObject();
        obj["ssid"] = WiFi.SSID(i);
        obj["rssi"] = WiFi.RSSI(i);
        obj["secured"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }

    String output;
    serializeJson(doc, output);
    return output;
}