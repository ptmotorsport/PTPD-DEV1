#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

enum LogLevel {
  LOG_NORMAL = 0,    // Only command responses and errors
  LOG_LEVEL1 = 1,    // + State changes and input events
  LOG_LEVEL2 = 2     // + CAN message details
};

class Logger {
public:
  static void init();
  static void setLevel(LogLevel level);
  static LogLevel getLevel();
  
  // Normal logging (always printed)
  static void printAlways(const String& msg);
  static void printAlways(const __FlashStringHelper* msg);
  
  // Level 1 logging - State changes and inputs
  static void printStateChange(const String& msg);
  static void printStateChange(const __FlashStringHelper* msg);
  static void printInput(const String& msg);
  static void printInput(const __FlashStringHelper* msg);
  
  // Level 2 logging - CAN messages
  static void printCANTx(uint32_t id, uint8_t* data, uint8_t len);
  static void printCANRx(uint32_t id, uint8_t* data, uint8_t len);
  
  // Utility functions
  static void printHex(uint8_t value);
  static void printHex(uint16_t value);
  static void printHex(uint32_t value);

private:
  Logger() = delete;
  static LogLevel currentLevel;
  static void printCANMessage(const char* direction, uint32_t id, uint8_t* data, uint8_t len);
};

// Convenience macros for easier usage
#define LOG_ALWAYS(msg) Logger::printAlways(msg)
#define LOG_STATE(msg) Logger::printStateChange(msg)
#define LOG_INPUT(msg) Logger::printInput(msg)
#define LOG_CAN_TX(id, data, len) Logger::printCANTx(id, data, len)
#define LOG_CAN_RX(id, data, len) Logger::printCANRx(id, data, len)

#endif
