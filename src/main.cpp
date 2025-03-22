#include <Arduino.h>
#include "config.h"
#include <SPI.h>
#include <SD.h>

// Variables for wheel RPM calculation
volatile unsigned long wheelPulseCount = 0;
unsigned long wheelLastTime = 0;
unsigned long wheelRPM = 0;
unsigned long wheelTotalRPM = 0;
unsigned long wheelReadingCount = 0;

// Variables for cadence calculation
volatile unsigned long cadencePulseCount = 0;
unsigned long cadenceLastTime = 0;
unsigned long cadenceRPM = 0;
unsigned long cadenceTotalRPM = 0;
unsigned long cadenceReadingCount = 0;

// Time tracking
unsigned long lastOutputTime = 0;
unsigned long lastLoggingTime = 0;
unsigned long sessionStartTime = 0;

// Session state
bool isSessionActive = false;
File logFile;
String logFileName = "";

// ISR for wheel sensor
void IRAM_ATTR wheelPulseCounter() {
  wheelPulseCount++;
}

// ISR for cadence sensor
void IRAM_ATTR cadencePulseCounter() {
  cadencePulseCount++;
}

// Create a new log file
bool createLogFile() {
  // Generate a timestamp for the filename
  unsigned long currentTime = millis();
  logFileName = String(LOG_FILE_PREFIX) + String(currentTime) + String(LOG_FILE_EXTENSION);
  
  // Open the file for writing
  logFile = SD.open(logFileName, FILE_WRITE);
  
  if (!logFile) {
    Serial.println("Error: Could not create log file!");
    return false;
  }
  
  // Write header row
  logFile.println("Timestamp,ElapsedTime(ms),WheelRPM,CadenceRPM");
  logFile.flush();
  
  Serial.print("Log file created: ");
  Serial.println(logFileName);
  return true;
}

// Start a new session
void startSession() {
  if (isSessionActive) {
    Serial.println("Session already active!");
    return;
  }
  
  if (createLogFile()) {
    isSessionActive = true;
    sessionStartTime = millis();
    Serial.println("Session started!");
  }
}

// Stop the current session
void stopSession() {
  if (!isSessionActive) {
    Serial.println("No active session!");
    return;
  }
  
  // Close the log file
  logFile.close();
  
  isSessionActive = false;
  Serial.println("Session stopped!");
  Serial.print("Data saved to: ");
  Serial.println(logFileName);
}

// Log data to the file
void logData(float wheelRPM, float cadenceRPM) {
  if (!isSessionActive) return;
  
  unsigned long currentTime = millis();
  unsigned long elapsedTime = currentTime - sessionStartTime;
  
  // Write data to log file
  logFile.print(currentTime);
  logFile.print(",");
  logFile.print(elapsedTime);
  logFile.print(",");
  logFile.print(wheelRPM, 1);
  logFile.print(",");
  logFile.println(cadenceRPM, 1);
  
  // Flush to ensure data is written to the card
  logFile.flush();
}

// Process serial commands
void processSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command == "start") {
      startSession();
    } else if (command == "stop") {
      stopSession();
    } else if (command == "status") {
      if (isSessionActive) {
        unsigned long elapsedTime = millis() - sessionStartTime;
        Serial.print("Session active for ");
        Serial.print(elapsedTime / 1000);
        Serial.println(" seconds");
      } else {
        Serial.println("No active session");
      }
    } else {
      Serial.println("Unknown command. Available commands: start, stop, status");
    }
  }
}

void setup() {
  // Initialize serial communication
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println("Turbo Trainer - Hall Sensor Test");
  
  // Set up Hall sensor pins as inputs with pullup resistors
  pinMode(WHEEL_SENSOR_PIN, INPUT_PULLUP);
  pinMode(CADENCE_SENSOR_PIN, INPUT_PULLUP);
  
  // Attach interrupts for the Hall sensors
  attachInterrupt(digitalPinToInterrupt(WHEEL_SENSOR_PIN), wheelPulseCounter, INTERRUPT_MODE);
  attachInterrupt(digitalPinToInterrupt(CADENCE_SENSOR_PIN), cadencePulseCounter, INTERRUPT_MODE);
  
  // Initialize SD card with custom pins
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  Serial.print("Initializing SD card...");
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD card initialization failed!");
  } else {
    Serial.println("SD card initialized.");
  }
  
  // Initialize timing
  wheelLastTime = millis();
  cadenceLastTime = millis();
  lastOutputTime = millis();
  lastLoggingTime = millis();
  
  Serial.println("Ready! Type 'start' to begin a new session.");
}

void loop() {
  unsigned long currentTime = millis();
  float avgWheelRPM = 0;
  float avgCadenceRPM = 0;
  
  // Calculate wheel RPM
  if (currentTime - wheelLastTime >= MEASUREMENT_INTERVAL) {
    // Calculate RPM: (pulses per second) * 60 / magnets
    wheelRPM = (wheelPulseCount * 60 * 1000) / ((currentTime - wheelLastTime) * WHEEL_MAGNETS);
    
    // Add to total for averaging
    wheelTotalRPM += wheelRPM;
    wheelReadingCount++;
    
    // Reset counter and update time
    wheelPulseCount = 0;
    wheelLastTime = currentTime;
  }
  
  // Calculate cadence RPM
  if (currentTime - cadenceLastTime >= MEASUREMENT_INTERVAL) {
    // Calculate RPM: (pulses per second) * 60 / magnets
    cadenceRPM = (cadencePulseCount * 60 * 1000) / ((currentTime - cadenceLastTime) * CRANK_MAGNETS);
    
    // Add to total for averaging
    cadenceTotalRPM += cadenceRPM;
    cadenceReadingCount++;
    
    // Reset counter and update time
    cadencePulseCount = 0;
    cadenceLastTime = currentTime;
  }
  
  // Output averaged readings every OUTPUT_INTERVAL milliseconds
  if (currentTime - lastOutputTime >= OUTPUT_INTERVAL) {
    // Calculate averages
    avgWheelRPM = wheelReadingCount > 0 ? (float)wheelTotalRPM / wheelReadingCount : 0;
    avgCadenceRPM = cadenceReadingCount > 0 ? (float)cadenceTotalRPM / cadenceReadingCount : 0;
    
    // Output to serial
    Serial.print("Wheel RPM: ");
    Serial.print(avgWheelRPM, 1);
    Serial.print(" | Cadence: ");
    Serial.print(avgCadenceRPM, 1);
    Serial.println(" RPM");
    
    // Reset counters for next averaging period
    wheelTotalRPM = 0;
    wheelReadingCount = 0;
    cadenceTotalRPM = 0;
    cadenceReadingCount = 0;
    
    lastOutputTime = currentTime;
  }
  
  // Log data at the specified interval
  if (isSessionActive && currentTime - lastLoggingTime >= LOGGING_INTERVAL) {
    // Calculate current averages for logging
    float currentWheelRPM = wheelReadingCount > 0 ? (float)wheelTotalRPM / wheelReadingCount : 0;
    float currentCadenceRPM = cadenceReadingCount > 0 ? (float)cadenceTotalRPM / cadenceReadingCount : 0;
    
    // Log the data
    logData(currentWheelRPM, currentCadenceRPM);
    
    lastLoggingTime = currentTime;
  }
  
  // Process any serial commands
  processSerialCommands();
}