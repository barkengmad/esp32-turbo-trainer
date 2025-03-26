#include "wifi_manager.h"

// Create the global instance
WiFiManager wifiManager;

WiFiManager::WiFiManager() :
    timeValid(false),
    lastSyncTime(0),
    espNowHandle(0)
{
    // Initialize controller MAC address (replace with your controller's MAC)
    uint8_t controllerMAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    memcpy(controllerAddress, controllerMAC, 6);
}

bool WiFiManager::begin() {
    // Initialize Wi-Fi in station mode
    WiFi.mode(WIFI_STA);
    
    // Connect to Wi-Fi
    Serial.print("Connecting to Wi-Fi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD); // WIFI_SSID and WIFI_PASSWORD are defined in secret.h
    
    // Wait for connection with timeout
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Failed to connect to Wi-Fi");
        return false;
    }
    
    Serial.println("Connected to Wi-Fi");
    
    // Sync time
    if (!syncTime()) {
        Serial.println("Failed to sync time");
        return false;
    }
    
    // Initialize ESP-NOW
    if (!initESPNow()) {
        Serial.println("Failed to initialize ESP-NOW");
        return false;
    }
    
    return true;
}

bool WiFiManager::syncTime() {
    // Configure NTP
    configTime(NTP_GMT_OFFSET_SEC, NTP_DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    
    // Wait for time to be set
    int attempts = 0;
    while (!timeValid && attempts < 10) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            timeValid = true;
            lastSyncTime = time(nullptr);
            Serial.println("Time synchronized successfully");
            return true;
        }
        delay(1000);
        attempts++;
    }
    
    Serial.println("Failed to get time from NTP");
    return false;
}

bool WiFiManager::initESPNow() {
    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return false;
    }
    
    // Register callback
    esp_now_register_send_cb(onDataSent);
    
    // Register peer
    memcpy(peerInfo.peer_addr, controllerAddress, 6);
    peerInfo.channel = ESPNOW_CHANNEL;
    peerInfo.encrypt = false;
    
    // Add peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return false;
    }
    
    Serial.println("ESP-NOW initialized successfully");
    return true;
}

bool WiFiManager::sendData(const SensorData& data) {
    // Send data via ESP-NOW
    esp_err_t result = esp_now_send(controllerAddress, (uint8_t*)&data, sizeof(SensorData));
    
    if (result != ESP_OK) {
        Serial.println("Error sending data");
        return false;
    }
    
    return true;
}

uint32_t WiFiManager::getCurrentTimestamp() const {
    if (!timeValid) {
        return 0;
    }
    return time(nullptr);
}

void WiFiManager::disconnectWiFi() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

void WiFiManager::onDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) {
        Serial.println("Error sending data");
    }
} 