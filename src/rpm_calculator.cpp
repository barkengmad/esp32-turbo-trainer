#include "rpm_calculator.h"

// Create the global instance
RPMCalculator rpmCalculator;

RPMCalculator::RPMCalculator() :
  wheelPulseCount(0),
  wheelLastTriggerTime(0),
  wheelTimeBetweenTriggers(0),
  instantWheelRPM(0),
  wheelTotalRPM(0),
  wheelReadingCount(0),
  wheelMagnets(1),
  cadencePulseCount(0),
  cadenceLastTriggerTime(0),
  cadenceTimeBetweenTriggers(0),
  instantCadenceRPM(0),
  cadenceTotalRPM(0),
  cadenceReadingCount(0),
  crankMagnets(1),
  sessionWheelTotalRPM(0),
  sessionWheelReadings(0),
  sessionAvgWheelRPM(0),
  sessionCadenceTotalRPM(0),
  sessionCadenceReadings(0),
  sessionAvgCadenceRPM(0),
  lastActivityTime(0),
  readingsStabilized(false),
  firstValidReadingTime(0),
  chainringCount(0),
  sprocketCount(0),
  currentChainring(0),
  currentSprocket(0),
  currentGearRatio(0),
  gearsConfigured(false)
{
  // Constructor initializes everything
}

void RPMCalculator::begin(uint8_t wheelMagnets, uint8_t crankMagnets) {
  this->wheelMagnets = wheelMagnets;
  this->crankMagnets = crankMagnets;
  reset();
  
  // Default gear configuration if none is provided - 2x9 road bike setup
  // This is just a placeholder and should be updated with configureGears
  uint8_t defaultChainrings[2] = {50, 34};  // 50/34 compact setup
  uint8_t defaultSprockets[9] = {11, 12, 13, 15, 17, 19, 21, 24, 28}; // Common 9-speed cassette
  configureGears(2, defaultChainrings, 9, defaultSprockets);
}

void RPMCalculator::configureGears(uint8_t chainringCount, const uint8_t* chainringTeeth, 
                                  uint8_t sprocketCount, const uint8_t* sprocketTeeth) {
  // Safety checks
  if (chainringCount > MAX_CHAINRINGS || sprocketCount > MAX_SPROCKETS) {
    return;
  }
  
  // Copy chainring teeth counts
  this->chainringCount = chainringCount;
  for (uint8_t i = 0; i < chainringCount; i++) {
    this->chainringTeeth[i] = chainringTeeth[i];
  }
  
  // Copy sprocket teeth counts
  this->sprocketCount = sprocketCount;
  for (uint8_t i = 0; i < sprocketCount; i++) {
    this->sprocketTeeth[i] = sprocketTeeth[i];
  }
  
  // Mark as configured
  gearsConfigured = true;
  
  // Reset current gear estimates
  currentChainring = 0;
  currentSprocket = 0;
  currentGearRatio = 0;
}

void RPMCalculator::estimateCurrentGear() {
  // Only estimate if wheels and cranks are moving
  if (!gearsConfigured || instantWheelRPM < 10 || instantCadenceRPM < 10) {
    currentChainring = 0;
    currentSprocket = 0;
    currentGearRatio = 0;
    return;
  }
  
  // Calculate the current gear ratio from RPM values
  float measuredRatio = instantWheelRPM / instantCadenceRPM;
  
  // Find the closest matching theoretical gear ratio
  float bestMatch = 99999.0;
  uint8_t bestChainring = 0;
  uint8_t bestSprocket = 0;
  
  for (uint8_t c = 0; c < chainringCount; c++) {
    for (uint8_t s = 0; s < sprocketCount; s++) {
      float theoreticalRatio = (float)chainringTeeth[c] / sprocketTeeth[s];
      float diff = abs(theoreticalRatio - measuredRatio);
      
      if (diff < bestMatch) {
        bestMatch = diff;
        bestChainring = c + 1;  // 1-based index
        bestSprocket = s + 1;   // 1-based index
        currentGearRatio = theoreticalRatio;
      }
    }
  }
  
  // Only update if confident (within 20% error)
  if (bestMatch / measuredRatio < 0.2) {
    currentChainring = bestChainring;
    currentSprocket = bestSprocket;
  }
}

String RPMCalculator::getGearDescription() const {
  if (!gearsConfigured || currentChainring == 0 || currentSprocket == 0) {
    return "Unknown Gear";
  }
  
  char buffer[20];
  uint8_t frontTeeth = chainringTeeth[currentChainring - 1];
  uint8_t rearTeeth = sprocketTeeth[currentSprocket - 1];
  
  snprintf(buffer, sizeof(buffer), "%d/%d (%.1f:1)", 
           frontTeeth, rearTeeth, currentGearRatio);
  
  return String(buffer);
}

void RPMCalculator::reset() {
  wheelPulseCount = 0;
  wheelLastTriggerTime = 0;
  wheelTimeBetweenTriggers = 0;
  instantWheelRPM = 0;
  wheelTotalRPM = 0;
  wheelReadingCount = 0;
  
  cadencePulseCount = 0;
  cadenceLastTriggerTime = 0;
  cadenceTimeBetweenTriggers = 0;
  instantCadenceRPM = 0;
  cadenceTotalRPM = 0;
  cadenceReadingCount = 0;
  
  lastActivityTime = millis();
  readingsStabilized = false;
  firstValidReadingTime = 0;
  
  // Don't reset gear configuration, just current estimates
  currentChainring = 0;
  currentSprocket = 0;
  currentGearRatio = 0;
}

void RPMCalculator::processWheelTrigger() {
  unsigned long currentTime = millis();
  
  // Add debounce protection - ignore triggers that happen too quickly
  if (currentTime - wheelLastTriggerTime < MIN_TRIGGER_TIME) {
    return; // Ignore triggers that are too close together (debounce)
  }
  
  // Record time and increment counter
  wheelTimeBetweenTriggers = currentTime - wheelLastTriggerTime;
  wheelLastTriggerTime = currentTime;
  wheelPulseCount++;
  lastActivityTime = currentTime;
}

void RPMCalculator::processCadenceTrigger() {
  unsigned long currentTime = millis();
  
  // Add debounce protection
  if (currentTime - cadenceLastTriggerTime < MIN_TRIGGER_TIME) {
    return;
  }
  
  // Record time and increment counter
  cadenceTimeBetweenTriggers = currentTime - cadenceLastTriggerTime;
  cadenceLastTriggerTime = currentTime;
  cadencePulseCount++;
  lastActivityTime = currentTime;
}

void RPMCalculator::calculateRPMs() {
  // Process wheel measurements
  if (wheelTimeBetweenTriggers > 0 && wheelTimeBetweenTriggers < MAX_TIME_BETWEEN_TRIGGERS) {
    float calculatedWheelRPM = (60.0 * 1000.0) / (wheelTimeBetweenTriggers * wheelMagnets);
    
    // Sanity check - only accept reasonable values
    if (calculatedWheelRPM <= MAX_WHEEL_RPM) {
      instantWheelRPM = calculatedWheelRPM;
      
      // Add to running average
      wheelTotalRPM += instantWheelRPM;
      wheelReadingCount++;
    }
  }
  
  // Process cadence measurements
  if (cadenceTimeBetweenTriggers > 0 && cadenceTimeBetweenTriggers < MAX_TIME_BETWEEN_TRIGGERS) {
    float calculatedCadenceRPM = (60.0 * 1000.0) / (cadenceTimeBetweenTriggers * crankMagnets);
    
    // Sanity check
    if (calculatedCadenceRPM <= MAX_CADENCE_RPM) {
      instantCadenceRPM = calculatedCadenceRPM;
      
      // Add to running average
      cadenceTotalRPM += instantCadenceRPM;
      cadenceReadingCount++;
    }
  }
  
  // Estimate current gear after calculating RPMs
  if (gearsConfigured && instantWheelRPM > 0 && instantCadenceRPM > 0) {
    estimateCurrentGear();
  }
}

void RPMCalculator::checkTimeouts() {
  unsigned long currentTime = millis();
  
  // Check for wheel timeout
  if (wheelLastTriggerTime > 0 && (currentTime - wheelLastTriggerTime) > TIMEOUT_PERIOD) {
    instantWheelRPM = 0;
    wheelLastTriggerTime = 0; // Reset to prevent repeated zeroing
  }
  
  // Check for cadence timeout
  if (cadenceLastTriggerTime > 0 && (currentTime - cadenceLastTriggerTime) > TIMEOUT_PERIOD) {
    instantCadenceRPM = 0;
    cadenceLastTriggerTime = 0; // Reset to prevent repeated zeroing
  }
  
  // If either wheel or cadence is zero, reset the gear estimate
  if (instantWheelRPM == 0 || instantCadenceRPM == 0) {
    currentChainring = 0;
    currentSprocket = 0;
    currentGearRatio = 0;
  }
}

void RPMCalculator::updateAverages() {
  // Update session-wide averages
  sessionWheelTotalRPM += wheelTotalRPM;
  sessionWheelReadings += wheelReadingCount;
  sessionAvgWheelRPM = sessionWheelReadings > 0 ? sessionWheelTotalRPM / sessionWheelReadings : 0;
  
  sessionCadenceTotalRPM += cadenceTotalRPM;
  sessionCadenceReadings += cadenceReadingCount;
  sessionAvgCadenceRPM = sessionCadenceReadings > 0 ? sessionCadenceTotalRPM / sessionCadenceReadings : 0;
}

void RPMCalculator::resetIntervalCounters() {
  wheelTotalRPM = 0;
  wheelReadingCount = 0;
  cadenceTotalRPM = 0;
  cadenceReadingCount = 0;
}

void RPMCalculator::startNewSession() {
  // Reset session averages
  sessionWheelTotalRPM = 0;
  sessionWheelReadings = 0;
  sessionAvgWheelRPM = 0;
  
  sessionCadenceTotalRPM = 0;
  sessionCadenceReadings = 0;
  sessionAvgCadenceRPM = 0;
  
  lastActivityTime = millis();
}

float RPMCalculator::getCurrentWheelRPM() const {
  return wheelReadingCount > 0 ? wheelTotalRPM / wheelReadingCount : 0;
}

float RPMCalculator::getCurrentCadenceRPM() const {
  return cadenceReadingCount > 0 ? cadenceTotalRPM / cadenceReadingCount : 0;
}

bool RPMCalculator::hasActivity() const {
  return (instantWheelRPM > 0 || instantCadenceRPM > 0);
}

bool RPMCalculator::areReadingsStabilized(unsigned long currentTime) {
  if (readingsStabilized) {
    return true;
  }
  
  if (wheelReadingCount >= 3 || cadenceReadingCount >= 3) {
    // We've had at least 3 valid readings, mark time of first stable reading
    if (firstValidReadingTime == 0) {
      firstValidReadingTime = currentTime;
    } else if (currentTime - firstValidReadingTime > STABILIZATION_PERIOD) {
      // If we've had stable readings for 2 seconds, consider readings stabilized
      readingsStabilized = true;
      return true;
    }
  } else {
    firstValidReadingTime = 0; // Reset if we don't have enough readings
  }
  
  return false;
}

void RPMCalculator::markActivity() {
  lastActivityTime = millis();
} 