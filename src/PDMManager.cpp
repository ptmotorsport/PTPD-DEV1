#include "PDMManager.h"
#include "CANHandler.h"
#include "Logger.h"
#include <EEPROM.h>
#include <Arduino.h>

// -----------------------------------------------------------------------------
// Temperature sensor: TMP235A2DBZR on A4
// Uncomment to bypass temperature sensor error detection while troubleshooting
// #define BYPASS_TEMP_SENSOR_ERROR

// -----------------------------------------------------------------------------
// EEPROM map & magic
static const int    ADDR_MAGIC             = 0;
static const uint16_t EEPROM_MAGIC         = 0xBEEF;
static const int    ADDR_CRC               = ADDR_MAGIC + sizeof(uint16_t);
static const int    ADDR_OC_ARRAY          = ADDR_CRC + sizeof(uint16_t);
static const int    ADDR_INRUSH_ARRAY      = ADDR_OC_ARRAY +       4*sizeof(float);
static const int    ADDR_INRUSHTIME_ARRAY  = ADDR_INRUSH_ARRAY +   4*sizeof(unsigned long);
static const int    ADDR_UNDERWARN_ARRAY   = ADDR_INRUSHTIME_ARRAY +4*sizeof(float);
static const int    ADDR_TEMPWARN          = ADDR_UNDERWARN_ARRAY +4*sizeof(float);
static const int    ADDR_TEMPTTRIP         = ADDR_TEMPWARN +       sizeof(float);
static const int    ADDR_MODE_ARRAY        = ADDR_TEMPTTRIP +      sizeof(float);
static const int    ADDR_GROUP_ARRAY       = ADDR_MODE_ARRAY +     4*sizeof(uint8_t);
static const int    ADDR_CAN_SPEED         = ADDR_GROUP_ARRAY +    4*sizeof(uint8_t);
static const int    ADDR_PDM_NODEID        = ADDR_CAN_SPEED   +    sizeof(uint8_t);
static const int    ADDR_KP_KEYNODE        = ADDR_PDM_NODEID  +    sizeof(uint8_t);

// -----------------------------------------------------------------------------
// Defaults
static float         ocThresholds[4]        = {3,3,3,3};
static float         inrushThresholds[4]    = {5,5,5,5};
static unsigned long inrushTimeLimits[4]    = {1000,1000,1000,1000};
static float         underWarnThresholds[4] = {0.10f,0.10f,0.10f,0.10f};
static float         tempWarnThreshold      = 70.0f;
static float         tempTripThreshold      = 85.0f;

static OutputMode    outputMode[4]    = {
  MODE_LATCH,MODE_LATCH,MODE_LATCH,MODE_LATCH
};
static uint8_t       outputGroup[4]   = {1,2,3,4};

static uint16_t      canSpeedKbps     = 1000;
static uint8_t       pdmNodeID        = 0x15;
static uint8_t       keypadNodeID     = 0x15;
uint16_t PDMManager::digitalOutCobId = 0x680;

// -----------------------------------------------------------------------------
// ADC scaling - Arduino Uno R4 Minima
static const float   voltageReference = 5.0f;   // Arduino Uno R4 Minima uses 5V reference for most analog pins
static const int     analogResolution = 1023;   // Arduino Uno R4 Minima is 10-bit ADC (0-1023)
static const float   ris              = 1000.0f;
static const float   kILIS            = 8200.0f; // Current sensor gain factor for DEV1.3 board (BTS443P)

// -----------------------------------------------------------------------------
// Dynamic
static float         overcurrentScore[4]    = {0,0,0,0};
static float         inrushScore[4]         = {0,0,0,0};
static unsigned long channelOnTime[4]       = {0,0,0,0};
static bool          channelActive[4]       = {false,false,false,false};
static bool          faultOvercurrent[4]    = {false,false,false,false};
static bool          warningUndercurrent[4] = {false,false,false,false};
static bool          faultThermal[4]        = {false,false,false,false};
static bool          clearedFault[4]        = {false,false,false,false};
static bool          resetButtonTiming[4]   = {false,false,false,false}; // Flag to reset button press timing when fault occurs
static LEDState      currentLEDStates[4]    = {
  LED_STATE_OFF,LED_STATE_OFF,LED_STATE_OFF,LED_STATE_OFF
};

static float         lastTemperature        = 0.0f;
static bool          lastSensorErr          = false;
static unsigned long lastUpdate             = 0;

// Temperature filtering variables
static float         filteredTemperature    = 25.0f;  // Start at reasonable room temp
static unsigned long lastTempUpdate         = 0;
static bool          tempSensorInitialized  = false;
static const float   maxTempChangePerSecond = 10.0f;  // 10°C/second max change rate
static uint8_t       badTempReadingCount    = 0;
static const uint8_t maxBadReadings         = 3;      // Require multiple bad readings before fault

// -----------------------------------------------------------------------------
// Digital switch input pins (replacing analog MUX)
static const uint8_t extSwitchPins[4] = {0, 1, 2, 3};  // D0-D3 for external switches
static const unsigned long extDebounceMs = 50;
static const uint8_t switchPins[4] = {6,9,10,11};  // D6, D9, D10, D11 for power outputs

// Current sensing pin mapping - DEV1.3 board layout
static const uint8_t currentSensePins[4] = {0, 1, 2, 3}; 

// -----------------------------------------------------------------------------
// CRC-16 helper functions for EEPROM validation
static uint16_t crc16_update(uint16_t crc, uint8_t data) {
  crc ^= data;
  for (uint8_t i = 0; i < 8; i++) {
    if (crc & 1) {
      crc = (crc >> 1) ^ 0xA001;  // CRC-16-IBM polynomial
    } else {
      crc >>= 1;
    }
  }
  return crc;
}

static uint16_t crc16_update_buffer(uint16_t crc, const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    crc = crc16_update(crc, data[i]);
  }
  return crc;
}

static uint16_t calculateConfigCRC() {
  uint16_t crc = 0xFFFF;
  
  // Hash all configuration data in the same order it's saved
  crc = crc16_update_buffer(crc, (const uint8_t*)ocThresholds, sizeof(ocThresholds));
  crc = crc16_update_buffer(crc, (const uint8_t*)inrushThresholds, sizeof(inrushThresholds));
  crc = crc16_update_buffer(crc, (const uint8_t*)inrushTimeLimits, sizeof(inrushTimeLimits));
  crc = crc16_update_buffer(crc, (const uint8_t*)underWarnThresholds, sizeof(underWarnThresholds));
  crc = crc16_update_buffer(crc, (const uint8_t*)&tempWarnThreshold, sizeof(tempWarnThreshold));
  crc = crc16_update_buffer(crc, (const uint8_t*)&tempTripThreshold, sizeof(tempTripThreshold));
  crc = crc16_update_buffer(crc, (const uint8_t*)outputMode, sizeof(outputMode));
  crc = crc16_update_buffer(crc, (const uint8_t*)outputGroup, sizeof(outputGroup));
  crc = crc16_update_buffer(crc, (const uint8_t*)&canSpeedKbps, sizeof(canSpeedKbps));
  crc = crc16_update_buffer(crc, (const uint8_t*)&pdmNodeID, sizeof(pdmNodeID));
  crc = crc16_update_buffer(crc, (const uint8_t*)&keypadNodeID, sizeof(keypadNodeID));
  
  return crc;
}

// Read digital switch inputs directly
static uint8_t getExtSwitchMask() {
  uint8_t mask = 0;
  for (uint8_t i = 0; i < 4; i++) {
    if (digitalRead(extSwitchPins[i]) == LOW) {  // Inverted: LOW = pressed (button pulls to ground)
      mask |= (1 << i);
    }
  }
  return mask;
}

// Group shutdown on fault
static void shutdownGroup(uint8_t ch) {
  uint8_t grp = outputGroup[ch];
  for (uint8_t i=0;i<4;i++){
    if (outputGroup[i] == grp) {
      channelActive[i] = false;
      faultOvercurrent[i] = true;
      digitalWrite(switchPins[i], LOW);
    }
  }
}

// Latch vs momentary + grouped press helper
static void applyPress(uint8_t ch, bool pressed) {
  uint8_t grp = outputGroup[ch];
  String debugMsg = "applyPress CH" + String(ch+1) + " pressed=" + String(pressed) + " group=" + String(grp);
  LOG_STATE(debugMsg);
  
  for (uint8_t i=0;i<4;i++){
    if (outputGroup[i] != grp) continue;
    bool isFaulted   = faultOvercurrent[i]||faultThermal[i];
    bool justCleared = clearedFault[i];

    String chDebug = "  CH" + String(i+1) + " mode=" + String(outputMode[i]) + 
                     " faulted=" + String(isFaulted) + " cleared=" + String(justCleared) + 
                     " active=" + String(channelActive[i]);
    LOG_STATE(chDebug);

    if (outputMode[i] == MODE_LATCH) {
      if (pressed) {
        // If faulted and not just cleared, skip this channel
        if (isFaulted && !justCleared) {
          LOG_STATE(F("  Skipping due to fault"));
          continue;
        }
        LOG_STATE(F("  Toggling channel"));
        PDMManager::setChannel(i, !channelActive[i]);
        // Clear the fault cleared flag after successful activation
        if (justCleared) clearedFault[i] = false;
      }
    } else {  // MOMENTARY mode
      if (isFaulted && !justCleared) {
        LOG_STATE(F("  Skipping due to fault (momentary)"));
        PDMManager::setChannel(i, false);
      } else {
        LOG_STATE(String("  Setting momentary channel to ") + String(pressed));
        PDMManager::setChannel(i, pressed);
        // Clear the fault cleared flag after successful activation
        if (justCleared) {
          clearedFault[i] = false;
          // Also clear the actual fault flags since they were cleared
          faultOvercurrent[i] = false;
          faultThermal[i] = false;
        }
      }
    }
  }
}

//------------------------------------------------------------------------------
// PDMManager public APIs

void PDMManager::init() {
  loadConfig();
  for (uint8_t i=0;i<4;i++){
    pinMode(switchPins[i], OUTPUT);
    digitalWrite(switchPins[i], LOW);
    channelActive[i]=false;
    faultOvercurrent[i]=false;
    warningUndercurrent[i]=false;
    faultThermal[i]=false;
    clearedFault[i]=false;
    currentLEDStates[i]=LED_STATE_OFF;
  }
  
  // Initialize digital switch input pins (D0-D3)
  for (uint8_t i=0;i<4;i++){
    pinMode(extSwitchPins[i], INPUT_PULLUP);  // Use internal pull-up resistors
  }
  
  // Analog pin configuration for new hardware:
  // A0-A3: Current sensing for channels 0-3
  // A4: Temperature sensor (corrected pin assignment)
  // A5: Battery voltage sensing (corrected pin assignment)
  pinMode(A4, INPUT);  // Temperature sensor
  pinMode(A5, INPUT);  // Battery voltage
  lastUpdate = millis();
}

void PDMManager::processExternalInputs() {
  static uint8_t  lastMask      = 0;
  static uint8_t  candidateMask = 0;
  static unsigned long changeTime = 0;
  static unsigned long pressStartExt[4] = {0,0,0,0};
  static bool          longDone[4]     = {false,false,false,false};
  unsigned long now = millis();

  // Check for fault-induced button timing resets
  for (uint8_t i = 0; i < 4; i++) {
    if (resetButtonTiming[i]) {
      pressStartExt[i] = now; // Reset press start time to now
      longDone[i] = false;    // Allow new long press detection
      resetButtonTiming[i] = false; // Clear the flag
      String resetMsg = "CH" + String(i+1) + " button timing reset due to fault";
      LOG_STATE(resetMsg);
    }
  }

  uint8_t raw = getExtSwitchMask();
  if (raw != candidateMask) {
    candidateMask = raw;
    changeTime = now;
    return;
  }
  if (now - changeTime < extDebounceMs) return;

  for (uint8_t ch=0; ch<4; ch++) {
    bool nowP = candidateMask & (1<<ch);
    bool wasP = lastMask      & (1<<ch);

    if (nowP && !wasP) {
      pressStartExt[ch]=now;
      longDone[ch]=false;
      String msg = "Ext CH" + String(ch+1) + " PRESSED";
      LOG_INPUT(msg);
      
      // For momentary mode, turn on immediately when pressed
      if (outputMode[ch] == MODE_MOMENTARY) {
        applyPress(ch, true);
        CANHandler::setLastInputMode(INPUT_MODE_DIGITAL);
      }
    }

    if (nowP && !longDone[ch] && now-pressStartExt[ch]>=1000) {
      // For LATCH mode: Always allow long press fault clearing
      // For MOMENTARY mode: Only allow long press fault clearing if channel is faulted
      bool allowLongPress = (outputMode[ch] == MODE_LATCH) || 
                           (outputMode[ch] == MODE_MOMENTARY && 
                            (faultOvercurrent[ch] || faultThermal[ch]));
      
      if (allowLongPress) {
        String msg = "Ext CH" + String(ch+1) + " LONG PRESS (fault clear)";
        LOG_INPUT(msg);
        for (uint8_t i=0;i<4;i++){
          if (outputGroup[i]==outputGroup[ch]) {
            faultOvercurrent[i]=false;
            faultThermal[i]=false;
            clearedFault[i]=true;
          }
        }
        shutdownGroup(ch);
        String stateMsg = "Group " + String(outputGroup[ch]) + " CLEARED";
        LOG_STATE(stateMsg);
        longDone[ch]=true;
      }
    }

    if (!nowP && wasP) {
      unsigned long dur = now-pressStartExt[ch];
      String msg = "Ext CH" + String(ch+1) + " RELEASED after " + String(dur) + " ms";
      LOG_INPUT(msg);
      
      if (!longDone[ch]) {
        if (outputMode[ch] == MODE_LATCH) {
          // For latch mode, toggle on short press release
          String shortMsg = "Ext CH" + String(ch+1) + " SHORT PRESS";
          LOG_INPUT(shortMsg);
          applyPress(ch, true);
          CANHandler::setLastInputMode(INPUT_MODE_DIGITAL);
        }
      } else {
        // If this was a long press (fault clear), don't do anything on release
        // The user will need to press again to activate the channel
        if (outputMode[ch] == MODE_MOMENTARY) {
          String clearMsg = "Ext CH" + String(ch+1) + " fault cleared - press again to activate";
          LOG_INPUT(clearMsg);
        }
      }
      
      // For momentary mode, turn off when released (but only if it wasn't a fault clearing long press)
      if (outputMode[ch] == MODE_MOMENTARY && !longDone[ch]) {
        applyPress(ch, false);
      }
    }
  }

  lastMask = candidateMask;
}

// -----------------------------------------------------------------------------
// Main update(): temperature, inrush + over-current fuses, LED states
void PDMManager::update() {
  unsigned long now = millis();
  float dt = (now - lastUpdate) / 1000.0f;
  lastUpdate = now;

  // --- Enhanced temperature sensor reading with filtering ---
  // TMP235A2DBZR: 10mV/°C, 500mV offset at 0°C
  // Temperature (°C) = (Vout - 0.5V) / 0.01V
  int rawT = analogRead(A4);  // Temperature sensor on A4
  float vT = rawT / float(analogResolution) * voltageReference;
  
  // Calculate temperature from TMP235A2DBZR
  // Valid output range: 0.1V (-40°C) to 2.0V (+150°C)
  float rawTemperature;
  if (vT < 0.05f || vT > 2.1f) {
    // Sensor disconnected or out of range
    rawTemperature = -999.0f;  // Invalid marker
  } else {
    // Convert voltage to temperature: T = (V - 0.5) / 0.01
    rawTemperature = (vT - 0.5f) / 0.01f;
  }
  
  // Initialize filtered temperature on first reading
  if (!tempSensorInitialized) {
    // Only initialize if the reading seems reasonable (-40°C to 150°C range for TMP235)
    if (rawTemperature >= -40.0f && rawTemperature <= 150.0f) {
      filteredTemperature = rawTemperature;
      tempSensorInitialized = true;
      lastTempUpdate = now;
      badTempReadingCount = 0;
      String initMsg = "Temperature sensor initialized at " + String(rawTemperature, 1) + "°C";
      LOG_STATE(initMsg);
    } else {
      // Use safe default until we get a good reading
      filteredTemperature = 25.0f;
      lastSensorErr = true;  // Mark as error until initialized
      lastTemperature = filteredTemperature;
      // Don't return - continue with rest of update function
    }
  }
  
  // Calculate time since last temperature update
  float tempDt = (now - lastTempUpdate) / 1000.0f;
  if (tempDt > 0.1f) {  // Only update every 100ms minimum
    
    // Check if raw reading is reasonable for TMP235 (-40°C to +150°C, 0.1V to 2.0V)
    bool rawReadingValid = (rawTemperature >= -40.0f && rawTemperature <= 150.0f && 
                           vT > 0.05f && vT < 2.1f);  // Voltage should be in valid range
    
    if (rawReadingValid) {
      // Calculate maximum allowed change based on time elapsed
      float maxChange = maxTempChangePerSecond * tempDt;
      float tempDiff = rawTemperature - filteredTemperature;
      
      // Limit the change rate
      if (abs(tempDiff) > maxChange) {
        // Large change detected - limit it
        if (tempDiff > 0) {
          filteredTemperature += maxChange;  // Heating up slowly
        } else {
          filteredTemperature -= maxChange;  // Cooling down slowly
        }
        
        String filterMsg = "Temp change limited: raw=" + String(rawTemperature, 1) + 
                          "°C, filtered=" + String(filteredTemperature, 1) + 
                          "°C, change=" + String(tempDiff, 1) + "°C";
        LOG_STATE(filterMsg);
      } else {
        // Normal change rate - use filtered value
        filteredTemperature = rawTemperature;
      }
      
      badTempReadingCount = 0;  // Reset bad reading counter
      lastSensorErr = false;
    } else {
      // Bad reading detected
      badTempReadingCount++;
      
      String badReadMsg = "Bad temp reading #" + String(badTempReadingCount) + 
                         ": raw=" + String(rawTemperature, 1) + 
                         "°C, voltage=" + String(vT, 3) + "V";
      LOG_STATE(badReadMsg);
      
      // Only declare sensor error after multiple consecutive bad readings
      if (badTempReadingCount >= maxBadReadings) {
        lastSensorErr = true;
        LOG_STATE("Temperature sensor error: too many bad readings");
      }
      // Keep using last good filtered temperature
    }
    
    lastTempUpdate = now;
  }
  
  // Use filtered temperature for all decisions
  float T = filteredTemperature;
  lastTemperature = T;  // Store filtered value for display

  // Enhanced sensor error detection
  bool sensorError = 
  #ifdef BYPASS_TEMP_SENSOR_ERROR
    false;
  #else
    lastSensorErr || (badTempReadingCount >= maxBadReadings);
  #endif
  
  // --- Per-channel logic ---
  for (uint8_t i = 0; i < 4; i++) {
    // 1) If we just cleared a fault, keep it off until next short-press
    if (clearedFault[i] && !channelActive[i]) {
      currentLEDStates[i] = LED_STATE_OFF;
      digitalWrite(switchPins[i], LOW);
      continue;
    }
    
    // Clear the fault flag if channel is active (successfully turned on)
    if (channelActive[i] && clearedFault[i]) {
      clearedFault[i] = false;
    }

    // 2) Thermal fault?
    if (sensorError) {
      LOG_STATE(F("Sensor pegged → thermal fault"));
      shutdownGroup(i);
      continue;
    }
    if (T >= tempTripThreshold) {
      String msg = "Ch " + String(i+1) + " Thermal trip";
      LOG_STATE(msg);
      shutdownGroup(i);
      continue;
    }
    if (T >= tempWarnThreshold) {
      String msg = "Warning: Temp " + String(T,1) + " C";
      LOG_STATE(msg);
    }

    // 3) If channel is off, choose its LED & continue
    if (!channelActive[i]) {
      if (faultThermal[i])          currentLEDStates[i] = LED_STATE_RED_FLASH;
      else if (faultOvercurrent[i]) currentLEDStates[i] = LED_STATE_RED;
      else                          currentLEDStates[i] = LED_STATE_OFF;
      digitalWrite(switchPins[i], LOW);
      continue;
    }

    // 4) Measure load current - using corrected pin mapping for current sensing
    int rawI = analogRead(A0 + currentSensePins[i]);  // Use mapped pins
    float vI = rawI / float(analogResolution) * voltageReference;
    float iA = vI / ris * kILIS;

    // undercurrent warning?
    warningUndercurrent[i] = (iA < underWarnThresholds[i]);

    // 5) Inrush‐window dynamic fuse
    unsigned long elapsed = now - channelOnTime[i];
    if (elapsed < inrushTimeLimits[i]) {
      if (iA > inrushThresholds[i]) {
        float ex = (iA / inrushThresholds[i]) - 1.0f;
        inrushScore[i] += dt * ex * ex;
        if (inrushScore[i] >= 1.0f) {
          String msg = "Channel " + String(i+1) + " Inrush fuse blown. Shutting down.";
          LOG_STATE(msg);
          setChannel(i, false);
          faultOvercurrent[i] = true;
          inrushScore[i] = 0.0f;
          continue;
        }
      } else {
        // back under inrush limit → reset
        inrushScore[i] = 0.0f;
      }
    }
    // 6) After inrush window, run over-current fuse
    else {
      if (iA > ocThresholds[i]) {
        float ex2 = (iA / ocThresholds[i]) - 1.0f;
        overcurrentScore[i] += dt * ex2 * ex2;
        if (overcurrentScore[i] >= 1.0f) {
          String msg = "Channel " + String(i+1) + " Overcurrent fuse blown. Shutting down.";
          LOG_STATE(msg);
          setChannel(i, false);
          faultOvercurrent[i] = true;
          overcurrentScore[i] = 0.0f;
          resetButtonTiming[i] = true; // Flag to reset button timing
          continue;
        }
      } else {
        overcurrentScore[i] = 0.0f;
      }
    }

    // 7) Channel is safe to drive ON
    digitalWrite(switchPins[i], HIGH);

    // 8) Set LED state based on current conditions
    if (faultOvercurrent[i]) {
      currentLEDStates[i] = LED_STATE_RED;
    }
    else if (warningUndercurrent[i]) {
      currentLEDStates[i] = LED_STATE_BLUE;
    }
    else if (iA > ocThresholds[i]) {
      // Over-current but fuse hasn't blown yet
      currentLEDStates[i] = LED_STATE_AMBER;
    }
    else {
      currentLEDStates[i] = LED_STATE_GREEN;
    }
  } // End of for loop
}


void PDMManager::setChannel(uint8_t ch, bool on) {
  channelActive[ch]=on;
  if (on) {
    channelOnTime[ch]=millis();
    overcurrentScore[ch]=0;
    faultOvercurrent[ch]=false;
    faultThermal[ch]=false;
    warningUndercurrent[ch]=false;
    clearedFault[ch]=false;
  }
  digitalWrite(switchPins[ch], on?HIGH:LOW);
}

void PDMManager::handleButtonState(uint8_t ch, bool pressed) {
  static bool     lastState[4]={false,false,false,false};
  static unsigned long pressTime[4]={0,0,0,0};
  unsigned long now = millis();
  bool was = lastState[ch];
  uint8_t grp = outputGroup[ch];

  // PRESS EDGE
  if (pressed && !was) {
    pressTime[ch]=now;
    String msg = "CAN CH" + String(ch+1) + " PRESSED";
    LOG_INPUT(msg);
    if (getOutputMode(ch)==MODE_MOMENTARY) {
      for (uint8_t i=0;i<4;i++){
        if (getOutputGroup(i)==grp) setChannel(i,true);
      }
    }
  }
  // RELEASE EDGE
  else if (!pressed && was) {
    unsigned long dur = now-pressTime[ch];
    String msg = "CAN CH" + String(ch+1) + " RELEASED after " + String(dur) + " ms";
    LOG_INPUT(msg);

    if (dur>=1000) {
      LOG_INPUT(F("→ LONG PRESS, clearing faults"));
      for (uint8_t i=0;i<4;i++){
        if (getOutputGroup(i)==grp) {
          faultOvercurrent[i]=false;
          faultThermal[i]=false;
          clearedFault[i]=true;
        }
      }
    } else {
      if (getOutputMode(ch)==MODE_LATCH) {
        bool groupFault=false;
        for (uint8_t i=0;i<4;i++){
          if (getOutputGroup(i)==grp &&
            (faultOvercurrent[i]||faultThermal[i])) {
            groupFault=true; break;
          }
        }
        if (!groupFault) {
          LOG_INPUT(F("→ SHORT PRESS, toggling group"));
          // toggle entire group
          bool anyOn=false;
          for (uint8_t i=0;i<4;i++){
            if (getOutputGroup(i)==grp && channelActive[i]) {
              anyOn=true; break;
            }
          }
          for (uint8_t i=0;i<4;i++){
            if (getOutputGroup(i)==grp) {
              setChannel(i, !anyOn);
            }
          }
        } else {
          LOG_INPUT(F("→ STILL FAULTED, ignoring short-press"));
        }
      }
    }
    if (getOutputMode(ch)==MODE_MOMENTARY) {
      for (uint8_t i=0;i<4;i++){
        if (getOutputGroup(i)==grp) setChannel(i,false);
      }
    }
  }

  lastState[ch]=pressed;
}

void PDMManager::getLEDStates(LEDState s[4]) {
  for (uint8_t i=0;i<4;i++) s[i]=currentLEDStates[i];
}

bool PDMManager::isChannelActive(uint8_t ch) {
  if (ch >= 4) return false;
  return channelActive[ch];
}

void PDMManager::saveConfig() {
  EEPROM.put(ADDR_MAGIC, EEPROM_MAGIC);
  for (uint8_t i=0;i<4;i++){
    EEPROM.put(ADDR_OC_ARRAY         + i*sizeof(float),          ocThresholds[i]);
    EEPROM.put(ADDR_INRUSH_ARRAY     + i*sizeof(float),      inrushThresholds[i]);
    EEPROM.put(ADDR_INRUSHTIME_ARRAY + i*sizeof(unsigned long), inrushTimeLimits[i]);
    EEPROM.put(ADDR_UNDERWARN_ARRAY  + i*sizeof(float),    underWarnThresholds[i]);
  }
  EEPROM.put(ADDR_TEMPWARN, tempWarnThreshold);
  EEPROM.put(ADDR_TEMPTTRIP,tempTripThreshold);
  for (uint8_t i=0;i<4;i++){
    EEPROM.put(ADDR_MODE_ARRAY   + i, (uint8_t)outputMode[i]);
    EEPROM.put(ADDR_GROUP_ARRAY  + i, (uint8_t)outputGroup[i]);
  }
  EEPROM.put(ADDR_CAN_SPEED,  (uint8_t)canSpeedKbps);
  EEPROM.put(ADDR_PDM_NODEID, pdmNodeID);
  EEPROM.put(ADDR_KP_KEYNODE, keypadNodeID);
  
  // Calculate and save CRC for data integrity verification
  uint16_t crc = calculateConfigCRC();
  EEPROM.put(ADDR_CRC, crc);
  
  Serial.print(F("OK: Configuration saved (CRC=0x"));
  Serial.print(crc, HEX);
  Serial.println(F(")"));
}

void PDMManager::loadConfig() {
  uint16_t m=0; EEPROM.get(ADDR_MAGIC,m);
  if (m==EEPROM_MAGIC) {
    // Load configuration data
    for (uint8_t i=0;i<4;i++){
      EEPROM.get(ADDR_OC_ARRAY         + i*sizeof(float),          ocThresholds[i]);
      EEPROM.get(ADDR_INRUSH_ARRAY     + i*sizeof(float),      inrushThresholds[i]);
      EEPROM.get(ADDR_INRUSHTIME_ARRAY + i*sizeof(unsigned long), inrushTimeLimits[i]);
      EEPROM.get(ADDR_UNDERWARN_ARRAY  + i*sizeof(float),    underWarnThresholds[i]);
    }
    EEPROM.get(ADDR_TEMPWARN,  tempWarnThreshold);
    EEPROM.get(ADDR_TEMPTTRIP, tempTripThreshold);
    for (uint8_t i=0;i<4;i++){
      uint8_t mm, gg;
      EEPROM.get(ADDR_MODE_ARRAY + i, mm);
      EEPROM.get(ADDR_GROUP_ARRAY+ i, gg);
      outputMode[i]  = (mm==MODE_MOMENTARY?MODE_MOMENTARY:MODE_LATCH);
      outputGroup[i] = gg;
    }
    uint8_t sp,p,k;
    EEPROM.get(ADDR_CAN_SPEED,  sp);
    EEPROM.get(ADDR_PDM_NODEID, p);
    EEPROM.get(ADDR_KP_KEYNODE, k);
    canSpeedKbps  = (sp==125||sp==250||sp==500||sp==1000)?sp:1000;
    pdmNodeID     = p;
    keypadNodeID  = k;
    
    // Verify CRC to ensure data integrity
    uint16_t storedCRC;
    EEPROM.get(ADDR_CRC, storedCRC);
    uint16_t calculatedCRC = calculateConfigCRC();
    
    if (storedCRC == calculatedCRC) {
      Serial.print(F("OK: Configuration loaded (CRC=0x"));
      Serial.print(storedCRC, HEX);
      Serial.println(F(")"));
    } else {
      Serial.print(F("WARN: Config CRC mismatch! Stored=0x"));
      Serial.print(storedCRC, HEX);
      Serial.print(F(", Calculated=0x"));
      Serial.print(calculatedCRC, HEX);
      Serial.println(F(" - Config may be corrupted, verify settings!"));
    }
  } else {
    Serial.println(F("INFO: No saved config."));
  }
}

void PDMManager::setOvercurrentThreshold(uint8_t ch, float a) {
  ocThresholds[ch]=a;
  Serial.print(F("OK: CH")); Serial.print(ch+1);
  Serial.print(F(" OC=")); Serial.print(a,2); Serial.println(F(" A"));
}
void PDMManager::setInrushThreshold(uint8_t ch, float a) {
  inrushThresholds[ch]=a;
  Serial.print(F("OK: CH")); Serial.print(ch+1);
  Serial.print(F(" INR=")); Serial.print(a,2); Serial.println(F(" A"));
}
void PDMManager::setInrushTimeLimit(uint8_t ch, unsigned long ms) {
  inrushTimeLimits[ch]=ms;
  Serial.print(F("OK: CH")); Serial.print(ch+1);
  Serial.print(F(" INRtime=")); Serial.print(ms); Serial.println(F(" ms"));
}
void PDMManager::setUndercurrentWarning(uint8_t ch, float a) {
  underWarnThresholds[ch]=a;
  Serial.print(F("OK: CH")); Serial.print(ch+1);
  Serial.print(F(" UWR=")); Serial.print(a,2); Serial.println(F(" A"));
}
void PDMManager::setTempWarnThreshold(float v) {
  tempWarnThreshold=v;
  Serial.print(F("OK: TempWarn=")); Serial.print(v,1); Serial.println(F(" C"));
}
void PDMManager::setTempTripThreshold(float v) {
  tempTripThreshold=v;
  Serial.print(F("OK: TempTrip=")); Serial.print(v,1); Serial.println(F(" C"));
}
float PDMManager::getTempWarnThreshold()  { return tempWarnThreshold; }
float PDMManager::getTempTripThreshold() { return tempTripThreshold; }

void PDMManager::setOutputMode(uint8_t ch, OutputMode m) {
  outputMode[ch]=m;
  Serial.print(F("OK: CH")); Serial.print(ch+1);
  Serial.print(F(" Mode="));
  Serial.println(m==MODE_LATCH?F("LATCH"):F("MOMENTARY"));
}
OutputMode PDMManager::getOutputMode(uint8_t ch) { return outputMode[ch]; }

void PDMManager::setOutputGroup(uint8_t ch, uint8_t g) {
  outputGroup[ch]=g;
  Serial.print(F("OK: CH")); Serial.print(ch+1);
  Serial.print(F(" Group=")); Serial.println(g);
}
uint8_t PDMManager::getOutputGroup(uint8_t ch) { return outputGroup[ch]; }

void PDMManager::setCANSpeed(uint16_t kbps) {
  if (kbps==125||kbps==250||kbps==500||kbps==1000) {
    canSpeedKbps=kbps;
    Serial.print(F("OK: CAN speed=")); Serial.print(kbps); Serial.println(F(" kbps"));
  } else {
    Serial.println(F("ERR: invalid CAN speed"));
  }
}
uint16_t PDMManager::getCANSpeed() { return canSpeedKbps; }

void PDMManager::setPDMNodeID(uint8_t id) {
  pdmNodeID=id;
  Serial.print(F("OK: PDM NodeID=0x")); Serial.println(id,HEX);
}
uint8_t PDMManager::getPDMNodeID() { return pdmNodeID; }

void PDMManager::setKeypadNodeID(uint8_t id) {
  keypadNodeID=id;
  Serial.print(F("OK: Keypad NodeID=0x")); Serial.println(id,HEX);
}
uint8_t PDMManager::getKeypadNodeID() { return keypadNodeID; }

float PDMManager::readBatteryVoltage() {
  int raw=analogRead(A5);  // Battery voltage sensing moved to A5 on new hardware
  float v = raw/float(analogResolution)*voltageReference;
  return v * 4.0f;  // divider 15k/5k = (15k+5k)/5k = 4.0
}
float PDMManager::getChannelCurrent(uint8_t ch) {
  int raw=analogRead(A0 + currentSensePins[ch]);  // Use mapped pins
  float v=raw/float(analogResolution)*voltageReference;
  return v/ris*kILIS;
}
bool PDMManager::isUndercurrentWarning(uint8_t ch) { 
  return warningUndercurrent[ch];
}
bool PDMManager::isOvercurrentFault(uint8_t ch) {
  return faultOvercurrent[ch];
}
bool PDMManager::isThermalFault(uint8_t ch) {
  return faultThermal[ch];
}

float PDMManager::getLastTemperature()  { return lastTemperature; }
bool  PDMManager::isTempSensorError()   { return lastSensorErr; }

void PDMManager::printConfig() {
  Serial.println(F("---- PDM Configuration ----"));
  for (uint8_t i=0;i<4;i++){
    Serial.print(F("CH"));Serial.print(i+1);
    Serial.print(F(": OC="));Serial.print(ocThresholds[i],2); Serial.print(F("A"));
    Serial.print(F(", INR="));Serial.print(inrushThresholds[i],2); Serial.print(F("A/"));
    Serial.print(inrushTimeLimits[i]); Serial.print(F("ms"));
    Serial.print(F(", UWR="));Serial.print(underWarnThresholds[i],2); Serial.print(F("A"));
    Serial.print(F(", Mode=")); Serial.print(outputMode[i]==MODE_LATCH?F("L"):F("M"));
    Serial.print(F(", Grp="));  Serial.println(outputGroup[i]);
  }
  Serial.print(F("TempWarn="));Serial.print(tempWarnThreshold,1);Serial.println(F(" C"));
  Serial.print(F("TempTrip="));Serial.print(tempTripThreshold,1);Serial.println(F(" C"));
  Serial.print(F("CAN Speed=")); Serial.print(canSpeedKbps); Serial.println(F(" kbps"));
  Serial.print(F("PDM NodeID=0x")); Serial.println(pdmNodeID,HEX);
  Serial.print(F("Keypad NodeID=0x")); Serial.println(keypadNodeID,HEX);
  Serial.print(F("CAN Rx Address=0x")); Serial.println(digitalOutCobId,HEX);
  Serial.println(F("---------------------------"));
}

void PDMManager::setDigitalOutID(uint16_t id) {
  digitalOutCobId = id;
  Serial.print(F("OK: DigitalOut COBID=0x"));
  Serial.println(id, HEX);
}

uint16_t PDMManager::getDigitalOutID() {
  return digitalOutCobId;
}

void PDMManager::clearChannelFault(uint8_t ch) {
  // clear any latched fault
  faultOvercurrent[ch]    = false;
  faultThermal[ch]        = false;
  warningUndercurrent[ch] = false;
  clearedFault[ch]        = true;
}