#include "Logger.h"

LogLevel Logger::currentLevel = LOG_NORMAL;

void Logger::init() {
  currentLevel = LOG_NORMAL;
}

void Logger::setLevel(LogLevel level) {
  currentLevel = level;
  Serial.print(F("LOG: Level set to "));
  switch (level) {
    case LOG_NORMAL:
      Serial.println(F("NORMAL (commands only)"));
      break;
    case LOG_LEVEL1:
      Serial.println(F("LEVEL1 (+ state changes & inputs)"));
      break;
    case LOG_LEVEL2:
      Serial.println(F("LEVEL2 (+ CAN messages)"));
      break;
  }
}

LogLevel Logger::getLevel() {
  return currentLevel;
}

void Logger::printAlways(const String& msg) {
  Serial.println(msg);
}

void Logger::printAlways(const __FlashStringHelper* msg) {
  Serial.println(msg);
}

void Logger::printStateChange(const String& msg) {
  if (currentLevel >= LOG_LEVEL1) {
    Serial.print(F("[STATE] "));
    Serial.println(msg);
  }
}

void Logger::printStateChange(const __FlashStringHelper* msg) {
  if (currentLevel >= LOG_LEVEL1) {
    Serial.print(F("[STATE] "));
    Serial.println(msg);
  }
}

void Logger::printInput(const String& msg) {
  if (currentLevel >= LOG_LEVEL1) {
    Serial.print(F("[INPUT] "));
    Serial.println(msg);
  }
}

void Logger::printInput(const __FlashStringHelper* msg) {
  if (currentLevel >= LOG_LEVEL1) {
    Serial.print(F("[INPUT] "));
    Serial.println(msg);
  }
}

void Logger::printCANTx(uint32_t id, uint8_t* data, uint8_t len) {
  if (currentLevel >= LOG_LEVEL2) {
    printCANMessage("TX", id, data, len);
  }
}

void Logger::printCANRx(uint32_t id, uint8_t* data, uint8_t len) {
  if (currentLevel >= LOG_LEVEL2) {
    printCANMessage("RX", id, data, len);
  }
}

void Logger::printCANMessage(const char* direction, uint32_t id, uint8_t* data, uint8_t len) {
  Serial.print(F("[CAN-"));
  Serial.print(direction);
  Serial.print(F("] ID:0x"));
  printHex(id);
  Serial.print(F(" LEN:"));
  Serial.print(len);
  Serial.print(F(" DATA:["));
  
  for (uint8_t i = 0; i < len; i++) {
    if (i > 0) Serial.print(F(","));
    Serial.print(F("0x"));
    printHex(data[i]);
  }
  Serial.println(F("]"));
}

void Logger::printHex(uint8_t value) {
  if (value < 0x10) Serial.print(F("0"));
  Serial.print(value, HEX);
}

void Logger::printHex(uint16_t value) {
  if (value < 0x1000) Serial.print(F("0"));
  if (value < 0x100) Serial.print(F("0"));
  if (value < 0x10) Serial.print(F("0"));
  Serial.print(value, HEX);
}

void Logger::printHex(uint32_t value) {
  if (value < 0x10000000) Serial.print(F("0"));
  if (value < 0x1000000) Serial.print(F("0"));
  if (value < 0x100000) Serial.print(F("0"));
  if (value < 0x10000) Serial.print(F("0"));
  if (value < 0x1000) Serial.print(F("0"));
  if (value < 0x100) Serial.print(F("0"));
  if (value < 0x10) Serial.print(F("0"));
  Serial.print(value, HEX);
}
