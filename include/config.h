#ifndef CONFIG_H
#define CONFIG_H

// Pin definitions for Hall sensors
#define WHEEL_SENSOR_PIN 15    // GPIO pin for wheel Hall sensor
#define CADENCE_SENSOR_PIN 16  // GPIO pin for cadence Hall sensor

// SD card module pin definitions
#define SD_CS_PIN 17            // CS pin for SD card module
#define SD_MOSI_PIN 18         // MOSI pin (default ESP32 VSPI)
#define SD_MISO_PIN 21         // MISO pin (default ESP32 VSPI)
#define SD_SCK_PIN 19         // SCK pin (default ESP32 VSPI)

// Serial configuration
#define SERIAL_BAUD_RATE 115200

// Timing configuration
#define OUTPUT_INTERVAL 3000    // Output interval in milliseconds
#define MEASUREMENT_INTERVAL 1000  // Interval for RPM calculations in milliseconds
#define LOGGING_INTERVAL 1000      // Interval for data logging in milliseconds

// Magnets configuration
#define WHEEL_MAGNETS 1  // Number of magnets on the wheel
#define CRANK_MAGNETS 1  // Number of magnets on the crank

// Sensor configuration
#define INTERRUPT_MODE FALLING  // Interrupt trigger mode (RISING, FALLING, CHANGE)

// Logging configuration
#define LOG_FILE_PREFIX "/session_"  // Prefix for log files
#define LOG_FILE_EXTENSION ".csv"    // File extension for log files

#endif // CONFIG_H 