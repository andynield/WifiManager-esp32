/*
   Custom WiFi Manager Implementation for ESP32
   Replacement for tzapu/WiFiManager
*/

#include "WiFiManagerCustom.h"
#include <Preferences.h>

static const char* STORAGE_KEY = "wifi_config";
static const char* STORAGE_NAMESPACE = "wifiManager";
static const int DNS_PORT = 53;
static const int CONFIG_SERVER_PORT = 80;

WiFiManagerCustom::WiFiManagerCustom() 
    : portalActive(false), blockingMode(true), debugOutput(false),
      configSaved(false), connectTimeout(20), configPortalTimeout(180),
      server(nullptr), dnsServer(nullptr) {
    memset(&config, 0, sizeof(config));
}

WiFiManagerCustom::~WiFiManagerCustom() {
    cleanupConfigPortal();
}

bool WiFiManagerCustom::autoConnect(const char* apName) {
    return autoConnect(apName, nullptr);
}

bool WiFiManagerCustom::autoConnect(const char* apName, const char* apPassword) {
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

bool WiFiManagerCustom::startConfigPortal(const char* apName) {
    return startConfigPortal(apName, nullptr);
}

bool WiFiManagerCustom::startConfigPortal(const char* apName, const char* apPassword) {
    // If device already has a saved config and can connect, don't start portal
    if (loadConfig() && connectToNetwork()) {
        if (debugOutput) {
            Serial.println("[WiFiManager] Device already configured and connected; not starting config portal");
        }
        return true;
    }
    
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

void WiFiManagerCustom::setupConfigPortal(const char* apName, const char* apPassword) {
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

void WiFiManagerCustom::setupDNS() {
    if (dnsServer == nullptr) {
        dnsServer = new DNSServer();
    }
    dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer->start(DNS_PORT, "*", WiFi.softAPIP());
}

void WiFiManagerCustom::process() {
    if (portalActive) {
        if (dnsServer) {
            dnsServer->processNextRequest();
        }
        if (server) {
            server->handleClient();
        }
    }
}

void WiFiManagerCustom::handleRoot() {
    server->send(200, "text/html", generateConfigHTML());
}

void WiFiManagerCustom::handleConfigRequest() {
    // Return current configuration
    StaticJsonDocument<512> doc;
    doc["ssid"] = config.ssid;
    doc["hostname"] = config.hostname;
    
    String response;
    serializeJson(doc, response);
    
    server->send(200, "application/json", response);
}

void WiFiManagerCustom::handleConfigSave() {
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

void WiFiManagerCustom::handleScanNetworks() {
    std::vector<String> networks = scanNetworks();
    String response = generateNetworkJSON(networks);
    server->send(200, "application/json", response);
}

void WiFiManagerCustom::handleStatus() {
    StaticJsonDocument<256> doc;
    doc["connected"] = WiFi.status() == WL_CONNECTED;
    doc["ssid"] = WiFi.SSID();
    doc["ip"] = WiFi.localIP().toString();
    doc["signal"] = WiFi.RSSI();
    
    String response;
    serializeJson(doc, response);
    
    server->send(200, "application/json", response);
}

void WiFiManagerCustom::handleNotFound() {
    // Redirect to root
    server->sendHeader("Location", "/", true);
    server->send(302, "text/plain", "");
}

void WiFiManagerCustom::startAccessPoint(const char* apName, const char* apPassword) {
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

void WiFiManagerCustom::stopAccessPoint() {
    WiFi.mode(WIFI_STA);
    WiFi.softAPdisconnect(true);
}

bool WiFiManagerCustom::connectToNetwork() {
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

std::vector<String> WiFiManagerCustom::scanNetworks() {
    std::vector<String> networks;
    
    int n = WiFi.scanNetworks();
    
    for (int i = 0; i < n; i++) {
        networks.push_back(WiFi.SSID(i));
    }
    
    return networks;
}

bool WiFiManagerCustom::loadConfig() {
    Preferences preferences;
    preferences.begin(STORAGE_NAMESPACE);
    
    String ssid = preferences.getString("ssid", "");
    String password = preferences.getString("password", "");
    String hostname = preferences.getString("hostname", "ESP32");
    
    preferences.end();
    
    if (ssid.length() > 0) {
        strncpy(config.ssid, ssid.c_str(), sizeof(config.ssid) - 1);
        strncpy(config.password, password.c_str(), sizeof(config.password) - 1);
        strncpy(config.hostname, hostname.c_str(), sizeof(config.hostname) - 1);
        return true;
    }

    return false;
}

bool WiFiManagerCustom::saveConfig() {
    // Save using Preferences (NVS)
    Preferences preferences;
    if (!preferences.begin(STORAGE_NAMESPACE, false)) {
        if (debugOutput) {
            Serial.println("[WiFiManager] Failed to open preferences for writing");
        }
        return false;
    }

    preferences.putString("ssid", String(config.ssid));
    preferences.putString("password", String(config.password));
    preferences.putString("hostname", String(config.hostname));

    preferences.end();
    return true;
}

void WiFiManagerCustom::cleanupConfigPortal() {
    if (dnsServer) {
        dnsServer->stop();
    }
    
    if (server) {
        server->stop();
        delete server;
        server = nullptr;
    }
    
    stopAccessPoint();
    portalActive = false;
}

String WiFiManagerCustom::generateConfigHTML() {
    return R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WiFi Configuration</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 20px;
        }
        .container {
            background: white;
            border-radius: 10px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.3);
            max-width: 500px;
            width: 100%;
            padding: 40px;
        }
        h1 {
            color: #333;
            margin-bottom: 10px;
            font-size: 28px;
        }
        .subtitle {
            color: #666;
            margin-bottom: 30px;
            font-size: 14px;
        }
        .form-group {
            margin-bottom: 20px;
        }
        label {
            display: block;
            margin-bottom: 8px;
            color: #333;
            font-weight: 500;
            font-size: 14px;
        }
        input[type="text"],
        input[type="password"],
        select {
            width: 100%;
            padding: 12px;
            border: 1px solid #ddd;
            border-radius: 5px;
            font-size: 14px;
            transition: border-color 0.3s;
        }
        input[type="text"]:focus,
        input[type="password"]:focus,
        select:focus {
            outline: none;
            border-color: #667eea;
            box-shadow: 0 0 0 3px rgba(102, 126, 234, 0.1);
        }
        select {
            cursor: pointer;
        }
        .network-list {
            background: #f8f9fa;
            border-radius: 5px;
            max-height: 200px;
            overflow-y: auto;
            margin-bottom: 10px;
        }
        .network-item {
            padding: 10px;
            border-bottom: 1px solid #eee;
            cursor: pointer;
            transition: background 0.2s;
        }
        .network-item:hover {
            background: #e9ecef;
        }
        .network-item:last-child {
            border-bottom: none;
        }
        .signal-strength {
            font-size: 12px;
            color: #666;
            margin-top: 5px;
        }
        .button-group {
            display: flex;
            gap: 10px;
            margin-top: 30px;
        }
        button {
            flex: 1;
            padding: 12px;
            border: none;
            border-radius: 5px;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s;
        }
        .btn-submit {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
        }
        .btn-submit:hover {
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(102, 126, 234, 0.4);
        }
        .btn-submit:active {
            transform: translateY(0);
        }
        .btn-scan {
            background: #f0f0f0;
            color: #333;
        }
        .btn-scan:hover {
            background: #e0e0e0;
        }
        .status {
            margin-top: 20px;
            padding: 15px;
            border-radius: 5px;
            font-size: 14px;
            display: none;
        }
        .status.success {
            background: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
            display: block;
        }
        .status.error {
            background: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
            display: block;
        }
        .status.loading {
            background: #cce5ff;
            color: #004085;
            border: 1px solid #b8daff;
            display: block;
        }
        .spinner {
            display: inline-block;
            width: 14px;
            height: 14px;
            border: 2px solid #f3f3f3;
            border-top: 2px solid #667eea;
            border-radius: 50%;
            animation: spin 0.8s linear infinite;
            margin-right: 8px;
            vertical-align: middle;
        }
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>⚙️ WiFi Configuration</h1>
        <p class="subtitle">Configure your device's WiFi connection</p>
        
        <form id="configForm">
            <div class="form-group">
                <label for="networkSelect">Available Networks:</label>
                <button id="scanBtn" type="button" class="btn-scan">🔍 Scan Networks</button>
                <div id="networkList" class="network-list"></div>
            </div>
            
            <div class="form-group">
                <label for="ssid">Network Name (SSID) *</label>
                <input type="text" id="ssid" name="ssid" placeholder="Enter SSID" required>
            </div>
            
            <div class="form-group">
                <label for="password">Password</label>
                <input type="password" id="password" name="password" placeholder="Enter WiFi password">
            </div>
            
            <div class="form-group">
                <label for="hostname">Device Hostname</label>
                <input type="text" id="hostname" name="hostname" placeholder="ESP32" value="ESP32">
            </div>
            
            <div id="status" class="status"></div>
            
            <div class="button-group">
                <button type="submit" class="btn-submit">💾 Save & Connect</button>
            </div>
        </form>
    </div>

    <script>
        const form = document.getElementById('configForm');
        const statusDiv = document.getElementById('status');
        const networkList = document.getElementById('networkList');
        // Use an explicit event listener for the scan button so HTML markup for the spinner is preserved
        const scanBtn = document.getElementById('scanBtn');
        if (scanBtn) scanBtn.addEventListener('click', scanNetworks);

        form.addEventListener('submit', async (e) => {
            e.preventDefault();
            
            const formData = new FormData(form);
            showStatus('loading', 'Saving configuration...');
            
            try {
                const response = await fetch('/save', {
                    method: 'POST',
                    body: formData
                });
                
                const data = await response.json();
                
                if (response.ok && data.status === 'saved') {
                    showStatus('success', '✓ Configuration saved! Device will reboot...');
                    setTimeout(() => {
                        location.reload();
                    }, 2000);
                } else {
                    showStatus('error', '✗ Failed to save configuration');
                }
            } catch (error) {
                showStatus('error', '✗ Error: ' + error.message);
            }
        });

        async function scanNetworks() {
            showStatus('loading', 'Scanning networks...');
            networkList.innerHTML = '';
            
            try {
                const response = await fetch('/scan');
                const data = await response.json();
                
                if (data.networks && data.networks.length > 0) {
                    data.networks.forEach(network => {
                        const item = document.createElement('div');
                        item.className = 'network-item';
                        item.innerHTML = `
                            <strong>${escapeHtml(network.ssid)}</strong>
                            <div class="signal-strength">Signal: ${network.rssi} dBm</div>
                        `;
                        item.addEventListener('click', () => {
                            document.getElementById('ssid').value = network.ssid;
                            document.getElementById('password').focus();
                        });
                        networkList.appendChild(item);
                    });
                    showStatus('success', '✓ Found ' + data.networks.length + ' networks');
                } else {
                    showStatus('error', '✗ No networks found');
                }
            } catch (error) {
                showStatus('error', '✗ Scan failed: ' + error.message);
            }
        }

        function showStatus(type, message) {
            // Build spinner element programmatically to avoid any HTML-escaping issues in some browsers
            statusDiv.className = 'status ' + type;
            // Clear previous content
            statusDiv.innerHTML = '';

            if (type === 'loading') {
                const spinner = document.createElement('span');
                spinner.className = 'spinner';
                statusDiv.appendChild(spinner);
                const text = document.createTextNode(' ' + message);
                statusDiv.appendChild(text);
            } else {
                statusDiv.textContent = message;
            }

            // Ensure visible and log for debugging on mobile browsers
            statusDiv.style.display = 'block';
            console.debug('showStatus:', type, message, 'innerHTML:', statusDiv.innerHTML);
        }

        function escapeHtml(text) {
            const map = {
                '&': '&amp;',
                '<': '&lt;',
                '>': '&gt;',
                '"': '&quot;',
                "'": '&#039;'
            };
            return text.replace(/[&<>\"']/g, m => map[m]);
        }

        // Load saved config on page load and then scan networks
        async function loadConfig() {
            try {
                const response = await fetch('/config');
                const data = await response.json();
                if (data.ssid) {
                    document.getElementById('ssid').value = data.ssid;
                }
                if (data.hostname) {
                    document.getElementById('hostname').value = data.hostname;
                }
            } catch (error) {
                console.error('Failed to load config:', error);
            }
            // Always scan for networks when the page loads
            scanNetworks();
        }

        loadConfig();
    </script>
</body>
</html>
    )";
}

String WiFiManagerCustom::generateNetworkJSON(const std::vector<String>& networks) {
    StaticJsonDocument<2048> doc;
    JsonArray arr = doc.createNestedArray("networks");
    
    for (size_t i = 0; i < networks.size(); i++) {
        JsonObject obj = arr.createNestedObject();
        obj["ssid"] = networks[i];
        obj["rssi"] = WiFi.RSSI(i);
    }
    
    String response;
    serializeJson(doc, response);
    
    return response;
}

void WiFiManagerCustom::setSaveConfigCallback(ConfigCallback callback) {
    onSaveConfigCallback = callback;
}

void WiFiManagerCustom::setHostname(const char* hostname) {
    strncpy(config.hostname, hostname, sizeof(config.hostname) - 1);
}

void WiFiManagerCustom::setConfigPortalBlocking(bool blocking) {
    blockingMode = blocking;
}

void WiFiManagerCustom::setDebugOutput(bool debug) {
    debugOutput = debug;
}

void WiFiManagerCustom::setConnectTimeout(uint16_t seconds) {
    connectTimeout = seconds;
}

void WiFiManagerCustom::setConfigPortalTimeout(uint16_t seconds) {
    configPortalTimeout = seconds;
}

void WiFiManagerCustom::disconnect() {
    WiFi.disconnect(true);
    stopAccessPoint();
}

String WiFiManagerCustom::getSSID() {
    return String(config.ssid);
}

String WiFiManagerCustom::getPassword() {
    return String(config.password);
}

bool WiFiManagerCustom::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

int WiFiManagerCustom::getRSSI(const char* ssid) {
    if (ssid == nullptr) {
        return WiFi.RSSI();
    }
    // Scan for specific SSID
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
        if (WiFi.SSID(i) == ssid) {
            return WiFi.RSSI(i);
        }
    }
    return -120;
}

String WiFiManagerCustom::getIP() {
    return WiFi.localIP().toString();
}

String WiFiManagerCustom::getMacAddress() {
    return WiFi.macAddress();
}

int WiFiManagerCustom::getSignalStrength() {
    return WiFi.RSSI();
}

bool WiFiManagerCustom::portalRunning() {
    return portalActive;
}

void WiFiManagerCustom::stopConfigPortal() {
    cleanupConfigPortal();
}