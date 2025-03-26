#ifndef RPM_CALCULATOR_H
#define RPM_CALCULATOR_H

#include <Arduino.h>

// Constants for RPM calculations
#define MAX_WHEEL_RPM 1000  // Maximum realistic wheel RPM
#define MAX_CADENCE_RPM 200 // Maximum realistic cadence RPM
#define MIN_TRIGGER_TIME 10 // Minimum ms between triggers (debounce)

// Maximum number of gears supported
#define MAX_CHAINRINGS 3
#define MAX_SPROCKETS 12

class RPMCalculator {
public:
    RPMCalculator();
    
    // Initialize the calculator
    void begin(uint8_t wheelMagnets, uint8_t crankMagnets);
    
    // Reset all stored values
    void reset();
    
    // Process a wheel sensor trigger
    void processWheelTrigger();
    
    // Process a cadence sensor trigger
    void processCadenceTrigger();
    
    // Calculate RPMs based on current data
    void calculateRPMs();
    
    // Check for timeouts (no recent triggers)
    void checkTimeouts();
    
    // Called to update averages at the reporting interval
    void updateAverages();
    
    // Reset the 3-second averaging counters
    void resetIntervalCounters();
    
    // Start a new session and reset session averages
    void startNewSession();
    
    // Getters for current values
    float getInstantWheelRPM() const { return instantWheelRPM; }
    float getInstantCadenceRPM() const { return instantCadenceRPM; }
    float getCurrentWheelRPM() const;
    float getCurrentCadenceRPM() const;
    float getSessionAvgWheelRPM() const { return sessionAvgWheelRPM; }
    float getSessionAvgCadenceRPM() const { return sessionAvgCadenceRPM; }
    
    // Activity detection
    bool hasActivity() const;
    bool areReadingsStabilized(unsigned long currentTime);
    
    // Other timing utilities
    void markActivity();
    unsigned long getLastActivityTime() const { return lastActivityTime; }

    // Gear estimation functions
    void configureGears(uint8_t chainringCount, const uint8_t* chainringTeeth, 
                        uint8_t sprocketCount, const uint8_t* sprocketTeeth);
    void estimateCurrentGear();
    uint8_t getCurrentChainring() const { return currentChainring; }
    uint8_t getCurrentSprocket() const { return currentSprocket; }
    float getCurrentGearRatio() const { return currentGearRatio; }
    String getGearDescription() const;

private:
    // Wheel variables
    volatile unsigned long wheelPulseCount;
    unsigned long wheelLastTriggerTime;
    unsigned long wheelTimeBetweenTriggers;
    float instantWheelRPM;
    float wheelTotalRPM;
    unsigned long wheelReadingCount;
    uint8_t wheelMagnets;
    
    // Cadence variables
    volatile unsigned long cadencePulseCount;
    unsigned long cadenceLastTriggerTime;
    unsigned long cadenceTimeBetweenTriggers;
    float instantCadenceRPM;
    float cadenceTotalRPM;
    unsigned long cadenceReadingCount;
    uint8_t crankMagnets;
    
    // Session average variables
    float sessionWheelTotalRPM;
    unsigned long sessionWheelReadings;
    float sessionAvgWheelRPM;
    float sessionCadenceTotalRPM;
    unsigned long sessionCadenceReadings;
    float sessionAvgCadenceRPM;
    
    // Activity variables
    unsigned long lastActivityTime;
    bool readingsStabilized;
    unsigned long firstValidReadingTime;
    
    // Gear variables
    uint8_t chainringCount;
    uint8_t chainringTeeth[MAX_CHAINRINGS];
    uint8_t sprocketCount;
    uint8_t sprocketTeeth[MAX_SPROCKETS];
    uint8_t currentChainring;  // 1-based index (1 = first chainring)
    uint8_t currentSprocket;   // 1-based index (1 = first sprocket)
    float currentGearRatio;
    bool gearsConfigured;
    
    // Constants
    const unsigned long TIMEOUT_PERIOD = 3000; // 3 seconds for timeout
    const unsigned long STABILIZATION_PERIOD = 2000; // 2 seconds for stabilization
    const unsigned long MAX_TIME_BETWEEN_TRIGGERS = 60000; // 60 seconds max between triggers
};

// Declare global instance
extern RPMCalculator rpmCalculator;

#endif // RPM_CALCULATOR_H 