#include "CANHandler.h"
#include "PDMManager.h"
#include "Logger.h"
#include <Arduino_CAN.h>

static const unsigned long WATCHDOG_TIMEOUT_MS = 1500;
static unsigned long lastDigOutTime = 0;

// CAN Digital Output state tracking
static bool lastDig[4] = {false,false,false,false};
static bool lockDig[4] = {false,false,false,false};

unsigned long CANHandler::lastHeartbeatMs = 0;
bool         CANHandler::_canOK          = true;
bool         CANHandler::_digOutWatchdogTriggered = false;
InputMode    CANHandler::lastInputMode   = INPUT_MODE_NONE;

void CANHandler::begin() {
  uint16_t kbps = PDMManager::getCANSpeed();
  
  // Initialize built-in CAN controller
  if (!CAN.begin(CanBitRate::BR_1000k)) {
    LOG_ALWAYS("CAN init FAILED");
    return;
  }
  
  // Set the correct bitrate based on configuration
  CanBitRate bitRate;
  switch (kbps) {
    case 125:  bitRate = CanBitRate::BR_125k; break;
    case 250:  bitRate = CanBitRate::BR_250k; break;
    case 500:  bitRate = CanBitRate::BR_500k; break;
    default:   bitRate = CanBitRate::BR_1000k; break;
  }
  
  // Restart with correct bitrate if different from default
  if (kbps != 1000) {
    CAN.end();
    if (!CAN.begin(bitRate)) {
      LOG_ALWAYS("CAN init FAILED");
      return;
    }
  }
  
  String msg = "CAN Initialized at " + String(kbps) + " kbps";
  LOG_ALWAYS(msg);

  // Start keypad & backlight & heartbeat
  uint8_t kpID = PDMManager::getKeypadNodeID();
  
  // Send start message
  uint8_t startMsg[2] = {0x01, kpID};
  sendMessage(0x00, startMsg, 2);
  String startMsgText = "Sent start message to keypad ID: 0x" + String(kpID, HEX);
  LOG_ALWAYS(startMsgText);
  
  // Small delay to ensure CAN bus is stable
  delay(10);
  
  // Send backlight setting
  sendBacklightSetting();
  LOG_ALWAYS("Sent backlight setting");
  
  // Small delay between messages
  delay(10);
  
  // Send heartbeat enable
  uint8_t hbEn[8] = {0x2B,0x17,0x10,0x00,0xF4,0x01,0x00,0x00};
  sendMessage(0x600 + kpID, hbEn, 8);
  String hbMsgText = "Sent heartbeat enable to COB-ID: 0x" + String(0x600 + kpID, HEX);
  LOG_ALWAYS(hbMsgText);
}
 
void CANHandler::process() {
  // 1) digital‐out timeout: if no DIGOUT msg in 2 s, shut all channels off
  // Only applies when last input was CAN DIGOUT
  if (lastInputMode == INPUT_MODE_CAN_DIGOUT && millis() - lastDigOutTime > 2000) {
    for (uint8_t ch = 0; ch < 4; ch++) {
      PDMManager::setChannel(ch, false);
      lastDig[ch] = false;  // Reset state so new ON commands will be recognized
    }
    LOG_STATE(F("CAN DIGOUT timeout: No digital output messages for 2+ seconds → outputs OFF, state reset"));
    _digOutWatchdogTriggered = true;  // Set watchdog flag for violet LED
    lastDigOutTime = millis();
  }

  // 2) check for incoming CAN frame
  if (CAN.available()) {
    // Read CAN message using Arduino_CAN library
    auto rxMsg = CAN.read();
    uint32_t id = rxMsg.id;
    uint8_t len = rxMsg.data_length;
    uint8_t* buf = (uint8_t*)rxMsg.data;

    // Log the received CAN message if level 2 logging is enabled
    LOG_CAN_RX(id, buf, len);

    // 3) DIGITAL‐OUT service (user-selectable COB-ID)
    {
        uint16_t digId = PDMManager::getDigitalOutID();
        if (id == digId && len >= 8) {
          bool newDig[4] = {
            (buf[0] & 0x01),
            (buf[2] & 0x01),
            (buf[4] & 0x01),
            (buf[6] & 0x01)
          };

          // Rising edge → try to turn ON (unless locked by a fault)
          for (uint8_t ch = 0; ch < 4; ch++) {
            if (newDig[ch] && !lastDig[ch] && !lockDig[ch]) {
              PDMManager::setChannel(ch, true);
              lastInputMode = INPUT_MODE_CAN_DIGOUT;  // Track input source
            }
            // Falling edge → turn OFF and clear lock
            else if (!newDig[ch] && lastDig[ch]) {
              PDMManager::setChannel(ch, false);
              lockDig[ch] = false;
              PDMManager::clearChannelFault(ch);
              lastInputMode = INPUT_MODE_CAN_DIGOUT;  // Track input source
            }
            lastDig[ch] = newDig[ch];
          }

          // update watchdog
          lastDigOutTime = millis();
          _digOutWatchdogTriggered = false;  // Clear watchdog flag when valid message received

          // capture any over-current/inrush fault and lock that channel
          for (uint8_t ch = 0; ch < 4; ch++) {
            if (PDMManager::isOvercurrentFault(ch)) {
              lockDig[ch] = true;
            }
          }

          // done with this frame
          return;
        }
      }

      // 4) KEYPAD handling
      uint8_t  kpID    = PDMManager::getKeypadNodeID();
      uint16_t pdoID   = 0x180 + kpID;
      uint16_t bootID  = 0x700 + kpID;

      // a) key-state PDO
      if (id == pdoID && len >= 1) {
        for (uint8_t ch = 0; ch < 4; ch++) {
          bool pressed = buf[0] & (1 << ch);
          PDMManager::handleButtonState(ch, pressed);
          if (pressed) {
            lastInputMode = INPUT_MODE_CAN_KEYPAD;  // Track input source
          }
        }
      }
      // b) boot-up notification
      else if (id == bootID && len >= 1 && buf[0] == 0x00) {
        String msg = "Keypad boot-up detected! Node ID: 0x" + String(kpID, HEX);
        LOG_ALWAYS(msg);
        
        uint8_t m1[2] = {0x01, kpID};
        sendMessage(0x000, m1, 2);
        LOG_ALWAYS("Sent start message in response to boot-up");
        
        sendBacklightSetting();
        LOG_ALWAYS("Sent backlight setting in response to boot-up");
        
        uint8_t hbEn[8] = {0x2B,0x17,0x10,0x00,0xF4,0x01,0x00,0x00};
        sendMessage(0x600 + kpID, hbEn, 8);
        LOG_ALWAYS("Sent heartbeat enable in response to boot-up");
      }
      // c) heartbeat "operational"
      else if (id == bootID && len >= 1 && buf[0] == 0x05) {
        lastHeartbeatMs = millis();
        _canOK = true;
      }

  } // end CAN.available
} // end CANHandler::process()

void CANHandler::sendMessage(uint32_t id, uint8_t* data, uint8_t len) {
  CanMsg txMsg;
  txMsg.id = id;
  txMsg.data_length = len;
  memcpy(txMsg.data, data, len);
  
  // Log the CAN message if level 2 logging is enabled
  LOG_CAN_TX(id, data, len);
  
  if (!CAN.write(txMsg)) {
    // Always report failures regardless of log level
    Serial.print("CAN TX FAILED: ID=0x");
    Serial.println(id, HEX);
  }
  // Remove the success message - it's now handled by LOG_CAN_TX at level 2
}

void CANHandler::sendKeypadLEDStatus(LEDState states[4]) {
  uint8_t data[8]={0};
  for (uint8_t i=0;i<4;i++){
    uint8_t b=1<<i;
    switch(states[i]) {
      case LED_STATE_GREEN: data[1]|=b; break;
      case LED_STATE_BLUE:  data[2]|=b; break;
      case LED_STATE_AMBER: data[0]|=b; data[1]|=b; break;
      case LED_STATE_RED:   data[0]|=b; break;
      default: break;
    }
  }
  uint8_t kpID = PDMManager::getKeypadNodeID();
  sendMessage(0x200 + kpID, data, 8);
}

void CANHandler::sendKeypadLEDBlinkStatus(LEDState states[4]) {
  uint8_t data[8]={0};
  for (uint8_t i=0;i<4;i++){
    if (states[i]==LED_STATE_RED_FLASH) data[0]|=(1<<i);
  }
  uint8_t kpID = PDMManager::getKeypadNodeID();
  sendMessage(0x300 + kpID, data, 8);
}

void CANHandler::sendBacklightSetting() {
  uint8_t data[8]={0};
  data[0]=0x0C; data[1]=0x07;
  uint8_t kpID = PDMManager::getKeypadNodeID();
  uint32_t msgID = 0x500 + kpID;
  
  if (Logger::getLevel() >= LOG_LEVEL2) {
    String msg = "Sending backlight setting: COB-ID=0x" + String(msgID, HEX) + 
                 ", data=[0x" + String(data[0], HEX) + ",0x" + String(data[1], HEX) + ",...]";
    LOG_ALWAYS(msg);
  }
  
  sendMessage(msgID, data, 8);
}

void CANHandler::sendTelemetry() {
  static unsigned long last = 0;
  if (millis() - last < 250) return;  // 4 Hz
  last = millis();

  uint8_t pdmID = PDMManager::getPDMNodeID();
  // base 0x380 + NodeID → 0x395 for NodeID=0x15
  uint32_t cob = 0x380 + pdmID;

  uint8_t data[8] = {0};

  // 1) Channel currents (bytes 0..3), 0.2 A/bit
  for (uint8_t i = 0; i < 4; i++) {
    float iA = PDMManager::getChannelCurrent(i);
    // scale: val = current / 0.2 = current * 5
    int v = int(round(iA * 5.0f));
    data[i] = (uint8_t) constrain(v, 0, 255);
  }

  // 2) Board temperature (byte 4), 1 degC per bit
  // assume PDMManager::getLastTemperature() returns float °C
  float T = PDMManager::getLastTemperature();
  data[4] = (uint8_t) constrain(int(round(T)), 0, 255);

  // 3) Fault mask (byte 5): bits 0–3 undercurrent, bits 4–7 overcurrent
  uint8_t flags = 0;
  for (uint8_t i = 0; i < 4; i++) {
    if (PDMManager::isUndercurrentWarning(i))  flags |= 1 << i;
    if (PDMManager::isOvercurrentFault(i))     flags |= 1 << (i + 4);
  }
  data[5] = flags;

  // 4) Battery voltage (bytes 6..7), 0.001 V/bit, little-endian
  float vb = PDMManager::readBatteryVoltage();  
  uint16_t vbit = uint16_t(round(vb * 1000.0f));
  data[6] = vbit & 0xFF;
  data[7] = vbit >> 8;

  // finally, transmit
  sendMessage(cob, data, 8);
}

void CANHandler::setLastInputMode(InputMode mode) {
  lastInputMode = mode;
}

InputMode CANHandler::getLastInputMode() {
  return lastInputMode;
}

bool CANHandler::isDigitalOutputWatchdogTriggered() {
  return _digOutWatchdogTriggered;
}

void CANHandler::checkWatchdog() {
  unsigned long now = millis();
  
  // Smart watchdog: only monitor the communication path that was last used
  switch (lastInputMode) {
    case INPUT_MODE_CAN_KEYPAD:
      // Only monitor CAN heartbeat if last input was from CAN keypad
      if (lastHeartbeatMs != 0 && now - lastHeartbeatMs > WATCHDOG_TIMEOUT_MS) {
        if (_canOK) {
          for (uint8_t i = 0; i < 4; i++) {
            PDMManager::setChannel(i, false);
          }
          LOG_STATE(F("Watchdog: CAN keypad lost → outputs OFF"));
        }
        _canOK = false;
      }
      break;
      
    case INPUT_MODE_CAN_DIGOUT:
      // DIGOUT timeout is handled in process() function
      // No additional watchdog needed here
      break;
      
    case INPUT_MODE_DIGITAL:
      // No watchdog for digital inputs - they're always local
      break;
      
    case INPUT_MODE_NONE:
    default:
      // No input received yet, or unknown mode - don't shutdown
      break;
  }
}

bool CANHandler::isCANOK() {
  // if the last PDM action was external, we still consider CAN OK
  //if (!PDMManager::wasLastUpdateByCAN()) return true;
  return _canOK;
}

//-------------------------------------------------------------------------
// CANHandler::setCANSpeed
// Re-initialize the bus at a new speed (in kbps: 125, 250, 500 or 1000).
void CANHandler::setCANSpeed(uint32_t speedKbps) {
  CanBitRate bitRate;
  switch (speedKbps) {
    case 125:  bitRate = CanBitRate::BR_125k;  break;
    case 250:  bitRate = CanBitRate::BR_250k;  break;
    case 500:  bitRate = CanBitRate::BR_500k;  break;
    case 1000: bitRate = CanBitRate::BR_1000k; break;
    default:
      Serial.println(F("ERR: CANSPEED 125|250|500|1000"));
      return;
  }

  // Re-initialize built-in CAN controller at the new speed
  CAN.end();
  if (CAN.begin(bitRate)) {
    Serial.print(F("OK: CAN speed "));
    Serial.print(speedKbps);
    Serial.println(F(" kbps"));
  } else {
    Serial.println(F("ERR: CAN init failed"));
  }
}
