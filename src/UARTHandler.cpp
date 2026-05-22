#include "UARTHandler.h"
#include "PDMManager.h"
#include "CANHandler.h"
#include "Logger.h"
#include <Arduino.h>

void UARTHandler::process() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length()==0) return;

  // Echo the received command for debugging
  Serial.print(F("Received: "));
  Serial.println(line);

  char buf[64];
  line.toCharArray(buf, sizeof(buf));
  char* tok = strtok(buf," ");
  String cmd = tok ? String(tok) : "";

  tok = strtok(NULL," ");
  String a1 = tok ? String(tok) : "";
  tok = strtok(NULL," ");
  String a2 = tok ? String(tok) : "";

  if (cmd=="OC") {
    uint8_t ch=a1.toInt()-1;
    PDMManager::setOvercurrentThreshold(ch, a2.toFloat());
  }
  else if (cmd=="INRUSH") {
    uint8_t ch=a1.toInt()-1;
    PDMManager::setInrushThreshold(ch, a2.toFloat());
  }
  else if (cmd=="INRUSHTIME") {
    uint8_t ch=a1.toInt()-1;
    PDMManager::setInrushTimeLimit(ch,a2.toInt());
  }
  else if (cmd=="UNDERWARN") {
    uint8_t ch=a1.toInt()-1;
    PDMManager::setUndercurrentWarning(ch, a2.toFloat());
  }
  else if (cmd=="TEMPWARN") {
    PDMManager::setTempWarnThreshold(a1.toFloat());
  }
  else if (cmd=="TEMPTRIP") {
    PDMManager::setTempTripThreshold(a1.toFloat());
  }
  else if (cmd=="MODE") {
    uint8_t ch=a1.toInt()-1;
    if (a2=="LATCH")      PDMManager::setOutputMode(ch,MODE_LATCH);
    else if (a2=="MOMENTARY") PDMManager::setOutputMode(ch,MODE_MOMENTARY);
    else Serial.println("ERR: MODE LATCH|MOMENTARY");
  }
  else if (cmd=="GROUP") {
    uint8_t ch=a1.toInt()-1;
    PDMManager::setOutputGroup(ch, a2.toInt());
  }
  else if (cmd=="CANSPEED") {
    PDMManager::setCANSpeed(a1.toInt());
  }
  else if (cmd=="NODEID") {
    if (a1=="PDM")     PDMManager::setPDMNodeID(strtol(a2.c_str(),NULL,0));
    else if (a1=="KEYPAD") PDMManager::setKeypadNodeID(strtol(a2.c_str(),NULL,0));
    else Serial.println("ERR: NODEID PDM|KEYPAD <hex|dec>");
  }

  else if (cmd == "DIGOUT") {
    // usage: DIGOUT <hex|dec>
    if (a1.length()) {
      uint16_t id = strtol(a1.c_str(), nullptr, 0);
      PDMManager::setDigitalOutID(id);
    } else {
      Serial.println("ERR: DIGOUT <hex|dec>");
    }
  }

  else if (cmd == "LOG") {
    // usage: LOG <level>
    if (a1.length()) {
      int level = a1.toInt();
      if (level >= 0 && level <= 2) {
        Logger::setLevel((LogLevel)level);
      } else {
        Serial.println("ERR: LOG 0|1|2 (0=Normal, 1=StateChanges, 2=+CAN)");
      }
    } else {
      Serial.print("Current log level: ");
      Serial.println((int)Logger::getLevel());
    }
  }

  else if (cmd == "TEMPRAW") {
    // Show raw TMP235 temperature data from ADS1115 AIN0.
    int16_t rawT = 0;
    float vT = 0.0f;
    float celsius = 0.0f;
    if (!PDMManager::readTemperatureRaw(rawT, vT, celsius)) {
      Serial.println("ERR: ADS1115 unavailable - cannot read temperature");
    } else {
      Serial.print("TMP235 on ADS AIN0 - Raw: ");
      Serial.print(rawT);
      Serial.print(", Voltage: ");
      Serial.print(vT, 3);
      Serial.print("V, Temperature: ");
      Serial.print(celsius, 1);
      Serial.println("C");
    }
  }

  else if (cmd == "TEMPDETAIL") {
    // Show detailed temperature sensor debug information
    Serial.println(F("=== Temperature Sensor Detail ==="));

    int16_t rawT = 0;
    float vT = 0.0f;
    float rawTemp = 0.0f;
    if (!PDMManager::readTemperatureRaw(rawT, vT, rawTemp)) {
      Serial.println(F("ERR: ADS1115 unavailable - cannot read temperature"));
    } else {
      Serial.print(F("Raw ADS: ")); Serial.print(rawT);
      Serial.print(F(", Voltage: ")); Serial.print(vT, 3); Serial.println(F("V"));
      Serial.print(F("Raw Temperature: ")); Serial.print(rawTemp, 2); Serial.println(F("C"));
    }
    Serial.print(F("Filtered Temperature: ")); Serial.print(PDMManager::getLastTemperature(), 2); Serial.println(F("°C"));
    Serial.print(F("Sensor Error: ")); Serial.println(PDMManager::isTempSensorError() ? "YES" : "NO");
    Serial.print(F("Battery Voltage: ")); Serial.print(PDMManager::readBatteryVoltage(), 2); Serial.println(F("V"));
    Serial.println(F("==============================="));
  }

  else if (cmd == "ANALOGRAW") {
    // Show raw current-sense ADC readings for this hardware without touching I2C pins A4/A5.
    Serial.println("Raw Analog Readings:");

    static const char* labels[NUM_CHANNELS] = {
      "IS1", "IS2", "IS3", "IS4", "IS5", "IS6", "IS7", "IS8", "IS9", "IS10"
    };

    for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
      int16_t raw = 0;
      float voltage = 0.0f;
      if (!PDMManager::readCurrentSenseRaw(ch, raw, voltage)) {
        Serial.print(labels[ch]);
        Serial.println(": ERR");
        continue;
      }

      Serial.print(labels[ch]);
      Serial.print(": "); Serial.print(raw);
      Serial.print(" ("); Serial.print(voltage, 3); Serial.println("V)");
    }
  }

  else if (cmd == "SWITCHRAW") {
    Serial.println("Raw Switch Readings:");
    for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
      bool pressed = false;
      if (!PDMManager::readSwitchInputRaw(ch, pressed)) {
        Serial.print("SW"); Serial.print(ch + 1); Serial.println(": ERR");
        continue;
      }

      Serial.print("SW"); Serial.print(ch + 1); Serial.print(": ");
      Serial.println(pressed ? "PRESSED" : "RELEASED");
    }

    uint16_t mask = PDMManager::readSwitchInputMaskRaw();
    Serial.print("Mask: 0b");
    for (int8_t bit = NUM_CHANNELS - 1; bit >= 0; bit--) {
      Serial.print((mask & (1 << bit)) ? '1' : '0');
    }
    Serial.print(" (0x");
    Serial.print(mask, HEX);
    Serial.println(")");
  }

  else if (cmd=="SHOW"||cmd=="PRINT") {
    PDMManager::printConfig();
  }
  else if (cmd=="SAVE") {
    PDMManager::saveConfig();
  }
  else if (cmd=="LOAD") {
    PDMManager::loadConfig();
  }
  else if (cmd=="STATUS") {
    Serial.println(F("===== PDM SYSTEM STATUS ====="));
    
    // System Information
    Serial.print(F("System Uptime: "));
    Serial.print(millis() / 1000);
    Serial.println(F(" seconds"));
    
    // Last Input Mode
    Serial.print(F("Last Input Mode: "));
    InputMode mode = CANHandler::getLastInputMode();
    switch (mode) {
      case INPUT_MODE_NONE:        Serial.println(F("NONE")); break;
      case INPUT_MODE_DIGITAL:     Serial.println(F("DIGITAL BUTTONS")); break;
      case INPUT_MODE_CAN_KEYPAD:  Serial.println(F("CAN KEYPAD")); break;
      case INPUT_MODE_CAN_DIGOUT:  Serial.println(F("CAN DIGITAL OUTPUT")); break;
      default:                     Serial.println(F("UNKNOWN")); break;
    }
    
    // CAN Status
    Serial.print(F("CAN Status: "));
    if (CANHandler::isCANOK()) {
      Serial.println(F("OK"));
    } else {
      Serial.println(F("TIMEOUT/ERROR"));
    }
    
    // Battery Voltage
    Serial.print(F("Battery Voltage: "));
    Serial.print(PDMManager::readBatteryVoltage(), 2);
    Serial.println(F(" V"));
    
    // Temperature
    Serial.print(F("Board Temperature: "));
    if (PDMManager::isTempSensorError()) {
      Serial.println(F("SENSOR ERROR"));
    } else {
      Serial.print(PDMManager::getLastTemperature(), 1);
      Serial.println(F(" °C"));
    }
    
    Serial.println(F(""));
    Serial.println(F("Channel Status:"));
    Serial.println(F("CH | ON/OFF | Current | Mode | Group | LED State | Warnings/Faults"));
    Serial.println(F("---|--------|---------|------|-------|-----------|------------------"));
    
    LEDState ledStates[NUM_CHANNELS];
    PDMManager::getLEDStates(ledStates);
    
    for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
      Serial.print(ch + 1);
      if (ch < 9) Serial.print(F(" "));  // Add space for single digit channels
      Serial.print(F(" | "));
      
      // Channel ON/OFF status
      if (PDMManager::isChannelActive(ch)) {
        Serial.print(F("  ON   | "));
      } else {
        Serial.print(F("  OFF  | "));
      }
      
      // Current reading
      Serial.print(PDMManager::getChannelCurrent(ch), 2);
      Serial.print(F(" A | "));
      
      // Mode
      if (PDMManager::getOutputMode(ch) == MODE_LATCH) {
        Serial.print(F(" L  | "));
      } else {
        Serial.print(F(" M  | "));
      }
      
      // Group
      Serial.print(F("  "));
      Serial.print(PDMManager::getOutputGroup(ch));
      Serial.print(F("   | "));
      
      // LED State
      switch (ledStates[ch]) {
        case LED_STATE_OFF:       Serial.print(F("   OFF   | ")); break;
        case LED_STATE_GREEN:     Serial.print(F("  GREEN  | ")); break;
        case LED_STATE_BLUE:      Serial.print(F("  BLUE   | ")); break;
        case LED_STATE_AMBER:     Serial.print(F("  AMBER  | ")); break;
        case LED_STATE_RED:       Serial.print(F("   RED   | ")); break;
        case LED_STATE_RED_FLASH: Serial.print(F("RED FLASH| ")); break;
        default:                  Serial.print(F(" UNKNOWN | ")); break;
      }
      
      // Fault status
      bool hasFaults = false;
      if (PDMManager::isOvercurrentFault(ch)) {
        Serial.print(F("OVERCURRENT "));
        hasFaults = true;
      }
      if (PDMManager::isThermalFault(ch)) {
        Serial.print(F("THERMAL "));
        hasFaults = true;
      }
      if (PDMManager::isUndercurrentWarning(ch)) {
        Serial.print(F("UNDERCURRENT "));
        hasFaults = true;
      }
      if (!hasFaults) {
        Serial.print(F("OK"));
      }
      Serial.println();
    }
    
    Serial.println(F("=============================="));
  }
  else if (cmd=="HELP" || cmd=="?") {
    Serial.println(F("===== PDM CLI Commands ====="));
    Serial.println(F("OC <ch> <amps>          - Set overcurrent threshold"));
    Serial.println(F("INRUSH <ch> <amps>      - Set inrush threshold"));
    Serial.println(F("INRUSHTIME <ch> <ms>    - Set inrush time limit"));
    Serial.println(F("UNDERWARN <ch> <amps>   - Set undercurrent warning"));
    Serial.println(F("TEMPWARN <temp>         - Set temperature warning"));
    Serial.println(F("TEMPTRIP <temp>         - Set temperature trip"));
    Serial.println(F("MODE <ch> LATCH|MOMENTARY - Set channel mode"));
    Serial.println(F("GROUP <ch> <group>      - Set channel group"));
    Serial.println(F("CANSPEED <kbps>         - Set CAN speed"));
    Serial.println(F("NODEID PDM|KEYPAD <id>  - Set node IDs"));
    Serial.println(F("DIGOUT <id>             - Set digital output CAN ID"));
    Serial.println(F("LOG <level>             - Set logging level (0=Normal, 1=State, 2=+CAN)"));
    Serial.println(F("TEMPRAW                 - Show raw TMP235 reading from ADS1115"));
    Serial.println(F("TEMPDETAIL              - Show detailed ADS temperature debug info"));
    Serial.println(F("ANALOGRAW               - Show raw IS1-IS10 current-sense ADC readings"));
    Serial.println(F("SWITCHRAW               - Show raw SW1-SW10 input states"));
    Serial.println(F("SHOW/PRINT              - Display configuration"));
    Serial.println(F("STATUS                  - Display system status"));
    Serial.println(F("SAVE                    - Save config to EEPROM"));
    Serial.println(F("LOAD                    - Load config from EEPROM"));
    Serial.println(F("HELP/?                  - Show this help"));
    Serial.println(F("============================"));
  }
  else {
    Serial.print(F("ERR: Unknown command '"));
    Serial.print(cmd);
    Serial.println(F("' - Type HELP for commands"));
  }
}
