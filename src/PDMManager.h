#ifndef PDM_MANAGER_H
#define PDM_MANAGER_H

#include <Arduino.h>

enum LEDState {
  LED_STATE_OFF = 0,
  LED_STATE_GREEN,
  LED_STATE_BLUE,
  LED_STATE_AMBER,
  LED_STATE_RED,
  LED_STATE_RED_FLASH
};

enum OutputMode {
  MODE_LATCH = 0,
  MODE_MOMENTARY
};

class PDMManager {
public:
  // Core
  static void init();
  static void processExternalInputs();
  static void update();
  static void printConfig();
  static void saveConfig();
  static void loadConfig();

  // Direct channel control
  static void setChannel(uint8_t ch, bool on);
  static void clearChannelFault(uint8_t ch);

  // CAN keypad integration
  static void handleButtonState(uint8_t ch, bool pressed);

  // Retrieve for CANHandler / dashboard
  static void getLEDStates(LEDState s[4]);
  static bool isChannelActive(uint8_t ch);    // Get channel ON/OFF status

  // CAN Msg Rx control (DIGOUT)
  static void    setDigitalOutID(uint16_t id);
  static uint16_t getDigitalOutID();

  // Threshold setters/getters
  static void setOvercurrentThreshold(uint8_t ch, float a);
  static void setInrushThreshold(uint8_t ch, float a);
  static void setInrushTimeLimit(uint8_t ch, unsigned long ms);
  static void setUndercurrentWarning(uint8_t ch, float a);
  static void setTempWarnThreshold(float v);
  static void setTempTripThreshold(float v);
  static float getTempWarnThreshold();
  static float getTempTripThreshold();

  // Mode & Group
  static void setOutputMode(uint8_t ch, OutputMode m);
  static OutputMode getOutputMode(uint8_t ch);
  static void setOutputGroup(uint8_t ch, uint8_t grp);
  static uint8_t getOutputGroup(uint8_t ch);

  // Additional CLI / EEPROM
  static void setCANSpeed(uint16_t kbps);
  static uint16_t getCANSpeed();
  static void setPDMNodeID(uint8_t id);
  static uint8_t getPDMNodeID();
  static void setKeypadNodeID(uint8_t id);
  static uint8_t getKeypadNodeID();

  // Telemetry helpers
  static float readBatteryVoltage();
  static float getChannelCurrent(uint8_t ch);
  static bool  isUndercurrentWarning(uint8_t ch);
  static bool  isOvercurrentFault(uint8_t ch);
  static bool  isThermalFault(uint8_t ch);

  // For NeoPixel
  static float getLastTemperature();
  static bool  isTempSensorError();

private:
  PDMManager() = delete;
  static uint16_t digitalOutCobId;
};

#endif
