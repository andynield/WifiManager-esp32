/*
   WiFi Manager Configuration Header
   Customize the WiFi Manager behavior here
*/

#ifndef WIFI_MANAGER_CONFIG_H
#define WIFI_MANAGER_CONFIG_H

// ============ Portal Configuration ============

// Maximum length for SSID name
#define MAX_SSID_LENGTH 32

// Maximum length for WiFi password
#define MAX_PASSWORD_LENGTH 64

// Maximum length for device hostname
#define MAX_HOSTNAME_LENGTH 32

// ============ Network Configuration ============

// DNS port for captive portal
#define WIFI_PORTAL_DNS_PORT 53

// Web server port for configuration
#define WIFI_PORTAL_SERVER_PORT 80

// ============ Timeout Configuration (in seconds) ============

// Default time to wait for WiFi connection
#define DEFAULT_CONNECT_TIMEOUT 20

// Default time portal stays active
#define DEFAULT_PORTAL_TIMEOUT 180

// Minimum connect timeout
#define MIN_CONNECT_TIMEOUT 5

// Maximum connect timeout
#define MAX_CONNECT_TIMEOUT 120

// Minimum portal timeout
#define MIN_PORTAL_TIMEOUT 60

// Maximum portal timeout
#define MAX_PORTAL_TIMEOUT 600

// ============ Storage Configuration ============

// NVS Namespace for credentials
#define WIFI_STORAGE_PATH "/wifi_config.json"

// NVS Key for SSID
#define WIFI_STORAGE_SSID_KEY "ssid"

// NVS Key for password
#define WIFI_STORAGE_PASS_KEY "password"

// NVS Key for hostname
#define WIFI_STORAGE_HOST_KEY "hostname"

// ============ Portal Features ============

// Enable network scanning feature
#define ENABLE_NETWORK_SCAN 1

// Enable signal strength display
#define ENABLE_SIGNAL_STRENGTH 1

// Enable hostname configuration
#define ENABLE_HOSTNAME_CONFIG 1

// Enable API endpoints
#define ENABLE_API_ENDPOINTS 1

// ============ Web UI Configuration ============

// Primary brand color (RGB hex without #)
#define PORTAL_COLOR_PRIMARY "667eea"

// Secondary brand color
#define PORTAL_COLOR_SECONDARY "764ba2"

// UI Theme ("light" or "dark")
#define PORTAL_THEME "light"

// Show signal strength indicator
#define SHOW_SIGNAL_INDICATOR 1

// ============ Debug Configuration ============

// Default debug state (change with setDebugOutput)
#define DEFAULT_DEBUG_OUTPUT 0

// Enable debug messages in WiFi Manager code
#define WIFI_DEBUG_MESSAGES 1

// ============ Security Configuration ============

// Require AP password (set to 1 for security, 0 for open)
#define REQUIRE_AP_PASSWORD 0

// Default AP password (only used if REQUIRE_AP_PASSWORD = 1)
#define DEFAULT_AP_PASSWORD "12345678"

// ============ Advanced Configuration ============

// DNS Redirect all requests to portal IP
#define DNS_REDIRECT_ALL 1

// Number of networks to show in scan results
#define MAX_NETWORK_RESULTS 20

// WiFi scan timeout in ms
#define WIFI_SCAN_TIMEOUT 10000

// JSON document size for ArduinoJson
#define JSON_DOC_SIZE 2048

// ============ Customization Hooks ============

// These can be defined in your sketch to customize behavior

// Called when entering config portal
// #define ON_PORTAL_START onPortalStart()

// Called when exiting config portal  
// #define ON_PORTAL_END onPortalEnd()

// Called when WiFi connects successfully
// #define ON_WIFI_CONNECTED onWiFiConnected()

// Called when WiFi connection fails
// #define ON_WIFI_FAILED onWiFiFailed()

#endif // WIFI_MANAGER_CONFIG_H
