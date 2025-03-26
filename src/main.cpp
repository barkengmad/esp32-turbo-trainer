#include <Arduino.h>
#include "config.h"
#include <SPI.h>
#include <SdFat.h>
#include "rpm_calculator.h"
#include "wifi_manager.h"

// Time tracking
unsigned long lastOutputTime = 0;
unsigned long lastLoggingTime = 0;
unsigned long sessionStartTime = 0;

// Session state
bool isSessionActive = false;
bool sdCardAvailable = false;

// Activity detection thresholds
#define ACTIVITY_TIMEOUT 5000  // 5 seconds without activity before stopping logging
#define INACTIVITY_CHECK_INTERVAL 1000  // Check for inactivity every second

// Gear configuration for this specific bike (2 chainrings, 9 sprockets)
// These values should be updated with actual tooth counts
const uint8_t CHAINRING_COUNT = 2;
const uint8_t CHAINRINGS[CHAINRING_COUNT] = {50, 34}; // Common road bike chainrings (front)

const uint8_t SPROCKET_COUNT = 9;
const uint8_t SPROCKETS[SPROCKET_COUNT] = {11, 12, 13, 15, 17, 19, 21, 24, 28}; // Common 9-speed cassette (rear)

// Create the objects
SdFat SD;
FsFile logFile;
String logFileName = "";

// ISR for wheel sensor - keep as simple as possible
void IRAM_ATTR wheelPulseCounter() {
  rpmCalculator.processWheelTrigger();
}

// ISR for cadence sensor - keep as simple as possible
void IRAM_ATTR cadencePulseCounter() {
  rpmCalculator.processCadenceTrigger();
}

// Create a new log file
bool createLogFile() {
  if (!sdCardAvailable) {
    Serial.println("Error: SD card not available");
    return false;
  }
  
  // Generate a timestamp for the filename using the current time
  uint32_t currentTime = wifiManager.getCurrentTimestamp();
  if (currentTime == 0) {
    // Fallback to millis() if time sync failed
    currentTime = millis();
  }
  logFileName = String(LOG_FILE_PREFIX) + String(currentTime) + String(LOG_FILE_EXTENSION);
  
  // Open the file for writing
  logFile = SD.open(logFileName.c_str(), FILE_WRITE);
  
  if (!logFile) {
    Serial.println("Error: Could not create log file!");
    return false;
  }
  
  // Write header row with session averages and gear info
  logFile.println("Timestamp,ElapsedTime(ms),WheelRPM,CadenceRPM,SessionAvgWheelRPM,SessionAvgCadenceRPM,Chainring,Sprocket,GearRatio");
  logFile.flush();
  
  Serial.print("Log file created: ");
  Serial.println(logFileName);
  return true;
}

// Start a new session
void startSession() {
  if (isSessionActive) {
    return;  // Session already active
  }
  
  // Reset session averages in the calculator
  rpmCalculator.startNewSession();
  
  // If SD card is available, create a log file
  if (sdCardAvailable) {
    if (createLogFile()) {
      isSessionActive = true;
      sessionStartTime = millis();
      Serial.println("Session automatically started with logging!");
    } else {
      // Even if logging fails, we still track the session
      isSessionActive = true;
      sessionStartTime = millis();
      Serial.println("Session started without logging (SD card error)");
    }
  } else {
    // Start session without logging
    isSessionActive = true;
    sessionStartTime = millis();
    Serial.println("Session started without logging (no SD card)");
  }
}

// Log data to the file
void logData(float wheelRPM, float cadenceRPM) {
  if (!isSessionActive || !sdCardAvailable) return;
  
  unsigned long currentTime = millis();
  unsigned long elapsedTime = currentTime - sessionStartTime;
  
  // Try to write data if possible
  if (logFile) {
    // Get current timestamp
    uint32_t timestamp = wifiManager.getCurrentTimestamp();
    if (timestamp == 0) {
      timestamp = currentTime; // Fallback to millis() if time sync failed
    }
    
    // Write data to log file with session averages and gear info
    logFile.print(timestamp);
    logFile.print(",");
    logFile.print(elapsedTime);
    logFile.print(",");
    logFile.print(wheelRPM, 1);
    logFile.print(",");
    logFile.print(cadenceRPM, 1);
    logFile.print(",");
    logFile.print(rpmCalculator.getSessionAvgWheelRPM(), 1);
    logFile.print(",");
    logFile.print(rpmCalculator.getSessionAvgCadenceRPM(), 1);
    logFile.print(",");
    logFile.print(rpmCalculator.getCurrentChainring());
    logFile.print(",");
    logFile.print(rpmCalculator.getCurrentSprocket());
    logFile.print(",");
    logFile.println(rpmCalculator.getCurrentGearRatio(), 2);
    
    // Flush data to SD card frequently to minimize data loss on power failure
    logFile.flush();
  }
  
  // Send data via ESP-NOW
  SensorData data;
  data.wheelRPM = wheelRPM;
  data.cadenceRPM = cadenceRPM;
  data.currentChainring = rpmCalculator.getCurrentChainring();
  data.currentSprocket = rpmCalculator.getCurrentSprocket();
  data.currentGearRatio = rpmCalculator.getCurrentGearRatio();
  data.timestamp = wifiManager.getCurrentTimestamp();
  
  wifiManager.sendData(data);
}

void setup() {
  // Initialize serial communication
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println("Turbo Trainer - Hall Sensor Test");
  delay(500);
  
  // Initialize Wi-Fi and time sync
  if (!wifiManager.begin()) {
    Serial.println("Failed to initialize Wi-Fi and time sync");
  }
  
  // Set up Hall sensor pins as inputs with pullup resistors
  pinMode(WHEEL_SENSOR_PIN, INPUT_PULLUP);
  pinMode(CADENCE_SENSOR_PIN, INPUT_PULLUP);
  
  // Initialize the RPM calculator with magnet counts from config
  rpmCalculator.begin(WHEEL_MAGNETS, CRANK_MAGNETS);
  
  // Configure the gears for this specific bike
  rpmCalculator.configureGears(CHAINRING_COUNT, CHAINRINGS, SPROCKET_COUNT, SPROCKETS);
  
  // Print gear configuration
  Serial.println("Gear Configuration:");
  Serial.print("Chainrings: ");
  for (uint8_t i = 0; i < CHAINRING_COUNT; i++) {
    Serial.print(CHAINRINGS[i]);
    if (i < CHAINRING_COUNT - 1) Serial.print(", ");
  }
  Serial.println(" teeth");
  
  Serial.print("Sprockets: ");
  for (uint8_t i = 0; i < SPROCKET_COUNT; i++) {
    Serial.print(SPROCKETS[i]);
    if (i < SPROCKET_COUNT - 1) Serial.print(", ");
  }
  Serial.println(" teeth");
  
  // Initialize timing
  lastOutputTime = millis();
  lastLoggingTime = millis();
  
  // Try to initialize SD card
  Serial.println("Trying to initialize SD card...");
  delay(500);
  
  // Initialize SD card - separate this from critical functionality
  Serial.print("Initializing SD card...");
  if (!SD.begin(SD_CONFIG)) {
    Serial.println("SD card initialization failed!");
    // Print more detailed diagnostics
    Serial.print("Error code: ");
    Serial.println(SD.sdErrorCode());
    Serial.print("Error data: ");
    Serial.println(SD.sdErrorData());
    sdCardAvailable = false;
  } else {
    Serial.println("SD card initialized successfully");
    delay(500); // Give the card time to stabilize
    
    // Check SD card type - use SdFat method
    Serial.print("SD Card present: ");
    if (SD.card()) {
      Serial.println("Yes");
      sdCardAvailable = true;
    } else {
      Serial.println("No");
      sdCardAvailable = false;
    }
  }
  
  // Now that other setup is done, attach interrupts last
  // This ensures all variables are initialized before interrupts can fire
  Serial.println("Attaching interrupt handlers...");
  attachInterrupt(digitalPinToInterrupt(WHEEL_SENSOR_PIN), wheelPulseCounter, INTERRUPT_MODE);
  attachInterrupt(digitalPinToInterrupt(CADENCE_SENSOR_PIN), cadencePulseCounter, INTERRUPT_MODE);
  
  // Disconnect from Wi-Fi after time sync
  wifiManager.disconnectWiFi();
  
  Serial.println("Ready! Waiting for movement to start recording.");
  if (!sdCardAvailable) {
    Serial.println("WARNING: SD card not available. Will function without logging.");
  }
}

void loop() {
  unsigned long currentTime = millis();
  
  // Process RPM calculations 
  rpmCalculator.calculateRPMs();
  
  // Check for timeouts
  rpmCalculator.checkTimeouts();
  
  // Handle output interval
  if (currentTime - lastOutputTime >= OUTPUT_INTERVAL) {
    // Update session averages if session is active
    if (isSessionActive) {
      rpmCalculator.updateAverages();
    }
    
    // Print session averages if active
    if (isSessionActive) {
      Serial.print("Session Avg - Wheel RPM: ");
      Serial.print(rpmCalculator.getSessionAvgWheelRPM(), 1);
      Serial.print(" | Cadence: ");
      Serial.print(rpmCalculator.getSessionAvgCadenceRPM(), 1);
      Serial.print(" RPM | ");
    }
    
    // Output current values to serial
    Serial.print("Current - Wheel RPM: ");
    Serial.print(rpmCalculator.getCurrentWheelRPM(), 1);
    Serial.print(" | Cadence: ");
    Serial.print(rpmCalculator.getCurrentCadenceRPM(), 1);
    Serial.print(" RPM");
    
    // Print current gear if available
    if (rpmCalculator.getCurrentChainring() > 0 && rpmCalculator.getCurrentSprocket() > 0) {
      Serial.print(" | Chainring ");
      Serial.print(rpmCalculator.getCurrentChainring());
      Serial.print(" (");
      Serial.print(CHAINRINGS[rpmCalculator.getCurrentChainring() - 1]);
      Serial.print(") : Sprocket ");
      Serial.print(rpmCalculator.getCurrentSprocket());
      Serial.print(" (");
      Serial.print(SPROCKETS[rpmCalculator.getCurrentSprocket() - 1]);
      Serial.print(")");
    }
    
    Serial.println();
    
    // Reset interval counters
    rpmCalculator.resetIntervalCounters();
    
    lastOutputTime = currentTime;
  }
  
  // Check if readings are stabilized and start session if there's activity
  if (!isSessionActive && rpmCalculator.areReadingsStabilized(currentTime) && rpmCalculator.hasActivity()) {
    startSession();
  }
  
  // Log data at the specified interval
  if (isSessionActive && currentTime - lastLoggingTime >= LOGGING_INTERVAL) {
    // Log the data with current values
    logData(rpmCalculator.getCurrentWheelRPM(), rpmCalculator.getCurrentCadenceRPM());
    
    lastLoggingTime = currentTime;
  }
}