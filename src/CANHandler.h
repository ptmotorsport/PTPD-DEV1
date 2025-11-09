#ifndef CAN_HANDLER_H
#define CAN_HANDLER_H

#include <Arduino.h>
#include <Arduino_CAN.h>
#include "PDMManager.h"

enum InputMode {
  INPUT_MODE_NONE,        // No input received yet
  INPUT_MODE_DIGITAL,     // Last input was from digital buttons
  INPUT_MODE_CAN_KEYPAD,  // Last input was from CAN keypad
  INPUT_MODE_CAN_DIGOUT   // Last input was from CAN digital output
};

class CANHandler {
public:
  static void    begin();
  static void    process();
  static void    sendTelemetry();
  static void    checkWatchdog();
  static bool    isCANOK();
  static bool    isDigitalOutputWatchdogTriggered();  // Check if CAN DIGOUT watchdog triggered
  static void    setCANSpeed(uint32_t speedKbps);
  static void   sendKeypadLEDStatus(LEDState states[4]);
  static void   sendKeypadLEDBlinkStatus(LEDState states[4]);
  static void   setLastInputMode(InputMode mode);  // Track input source
  static InputMode getLastInputMode();              // Get current input mode

private:
  CANHandler() = delete;
  static void   sendMessage(uint32_t id, uint8_t* data, uint8_t len);

  static void   sendBacklightSetting();
  static unsigned long lastHeartbeatMs; // last time we saw a "05" heartbeat
  static bool          _canOK;          // whether heartbeat is still within timeout
  static bool          _digOutWatchdogTriggered;  // track CAN DIGOUT watchdog state
  static InputMode     lastInputMode;   // Track which input was used last
};

#endif
