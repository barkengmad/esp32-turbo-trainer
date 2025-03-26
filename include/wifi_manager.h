#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <time.h>
#include "secret.h"

// NTP settings
#define NTP_SERVER "pool.ntp.org"
#define NTP_GMT_OFFSET_SEC 0  // Adjust based on your timezone
#define NTP_DAYLIGHT_OFFSET_SEC 0

// ESP-NOW settings
#define ESPNOW_CHANNEL 1
#define ESPNOW_PMK "pmk1234567890123"
#define ESPNOW_LMK "lmk1234567890123"

// Message structure for ESP-NOW communication
struct SensorData {
    float wheelRPM;
    float cadenceRPM;
    uint8_t currentChainring;
    uint8_t currentSprocket;
    float currentGearRatio;
    uint32_t timestamp;
};

class WiFiManager {
public:
    WiFiManager();
    
    // Initialize Wi-Fi and time sync
    bool begin();
    
    // Sync time with NTP server
    bool syncTime();
    
    // Initialize ESP-NOW
    bool initESPNow();
    
    // Send data via ESP-NOW
    bool sendData(const SensorData& data);
    
    // Get current timestamp
    uint32_t getCurrentTimestamp() const;
    
    // Disconnect from Wi-Fi
    void disconnectWiFi();
    
    // Check if time is valid
    bool isTimeValid() const { return timeValid; }

private:
    // ESP-NOW callback
    static void onDataSent(const uint8_t* mac_addr, esp_now_send_status_t status);
    
    // ESP-NOW peer info
    esp_now_peer_info_t peerInfo;
    uint8_t controllerAddress[6];
    
    // Time sync status
    bool timeValid;
    time_t lastSyncTime;
    
    // ESP-NOW interface
    esp_now_handle_t espNowHandle;
};

// Declare global instance
extern WiFiManager wifiManager;

#endif // WIFI_MANAGER_H 