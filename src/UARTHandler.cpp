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
    // Show raw temperature sensor data for LM335 with 2kΩ pull-up
    int rawT = analogRead(A4);
    float vT = rawT / 1023.0f * 5.0f;     // Voltage at A4
    
    // LM335 with 2kΩ pull-up voltage divider calculation
    float resistance_lm335 = (2000.0f * vT) / (5.0f - vT);
    float kelvin = resistance_lm335 / 10.0f;  // LM335: R ≈ 10Ω per Kelvin
    float celsius = kelvin - 273.15f;
    
    Serial.print("LM335 + 2kΩ pullup - Raw: ");
    Serial.print(rawT);
    Serial.print("/1023, Voltage: ");
    Serial.print(vT, 3);
    Serial.println("V");
    
    Serial.print("LM335 Resistance: ");
    Serial.print(resistance_lm335, 0);
    Serial.print("Ω, Temperature: ");
    Serial.print(celsius, 1);
    Serial.println("°C");
    
    // Expected at 25°C: R=2980Ω, V=2.99V, Raw=611
    Serial.println("Expected 25°C: R=2980Ω, V=2.99V, Raw=611");
  }

  else if (cmd == "TEMPDETAIL") {
    // Show detailed temperature sensor debug information
    Serial.println(F("=== Temperature Sensor Detail ==="));
    
    // Read raw values
    int rawT = analogRead(A4);
    float vT = rawT / 1023.0f * 5.0f;
    float resistance = (2000.0f * vT) / (5.0f - vT);
    float kelvin = resistance / 10.0f;
    float rawTemp = kelvin - 273.15f;
    
    Serial.print(F("Raw ADC: ")); Serial.print(rawT); Serial.print(F("/1023"));
    Serial.print(F(", Voltage: ")); Serial.print(vT, 3); Serial.println(F("V"));
    Serial.print(F("LM335 Resistance: ")); Serial.print(resistance, 1); Serial.println(F(" ohms"));
    Serial.print(F("Raw Temperature: ")); Serial.print(rawTemp, 2); Serial.println(F("°C"));
    Serial.print(F("Filtered Temperature: ")); Serial.print(PDMManager::getLastTemperature(), 2); Serial.println(F("°C"));
    Serial.print(F("Sensor Error: ")); Serial.println(PDMManager::isTempSensorError() ? "YES" : "NO");
    Serial.print(F("Battery Voltage: ")); Serial.print(PDMManager::readBatteryVoltage(), 2); Serial.println(F("V"));
    Serial.println(F("==============================="));
  }

  else if (cmd == "ANALOGRAW") {
    // Show all analog readings for debugging
    Serial.println("Raw Analog Readings:");
    for (int i = 0; i <= 5; i++) {
      int raw = analogRead(A0 + i);
      float voltage = raw / 1023.0f * 5.0f;
      Serial.print("A"); Serial.print(i); 
      Serial.print(": "); Serial.print(raw);
      Serial.print(" ("); Serial.print(voltage, 3); Serial.println("V)");
    }
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
    
    LEDState ledStates[4];
    PDMManager::getLEDStates(ledStates);
    
    for (uint8_t ch = 0; ch < 4; ch++) {
      Serial.print(ch + 1);
      Serial.print(F("  | "));
      
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
    Serial.println(F("TEMPRAW                 - Show raw temperature sensor data"));
    Serial.println(F("TEMPDETAIL              - Show detailed temperature sensor debug info"));
    Serial.println(F("ANALOGRAW               - Show all analog pin readings"));
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
