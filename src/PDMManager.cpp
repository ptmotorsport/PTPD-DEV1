#include "PDMManager.h"
#include "CANHandler.h"
#include "Logger.h"
#include <EEPROM.h>
#include <Arduino.h>
#include <Adafruit_ADS1X15.h>
#include "bsp_api.h"
#include "r_adc.h"
#include "hal_data.h"
#include "r_ioport.h"

// -----------------------------------------------------------------------------
// ADS1115 I2C ADC for temperature, battery voltage, IS9, IS10
Adafruit_ADS1115 ads;  // Default I2C address 0x48 (1001000b)
static bool adsInitialized = false;
static float vbattCalibration = 1.0f;  // Calibration factor for battery voltage

// -----------------------------------------------------------------------------
// Temperature sensor: TMP235A2DBZR on ADS1115 AIN0
// Uncomment to bypass temperature sensor error detection while troubleshooting
// #define BYPASS_TEMP_SENSOR_ERROR

// -----------------------------------------------------------------------------
// EEPROM map & magic
static const int    ADDR_MAGIC             = 0;
static const uint16_t EEPROM_MAGIC         = 0xBEEF;
static const int    ADDR_CRC               = ADDR_MAGIC + sizeof(uint16_t);
static const int    ADDR_OC_ARRAY          = ADDR_CRC + sizeof(uint16_t);
static const int    ADDR_INRUSH_ARRAY      = ADDR_OC_ARRAY +       NUM_CHANNELS*sizeof(float);
static const int    ADDR_INRUSHTIME_ARRAY  = ADDR_INRUSH_ARRAY +   NUM_CHANNELS*sizeof(unsigned long);
static const int    ADDR_UNDERWARN_ARRAY   = ADDR_INRUSHTIME_ARRAY +NUM_CHANNELS*sizeof(float);
static const int    ADDR_TEMPWARN          = ADDR_UNDERWARN_ARRAY +NUM_CHANNELS*sizeof(float);
static const int    ADDR_TEMPTTRIP         = ADDR_TEMPWARN +       sizeof(float);
static const int    ADDR_MODE_ARRAY        = ADDR_TEMPTTRIP +      sizeof(float);
static const int    ADDR_GROUP_ARRAY       = ADDR_MODE_ARRAY +     NUM_CHANNELS*sizeof(uint8_t);
static const int    ADDR_CAN_SPEED         = ADDR_GROUP_ARRAY +    NUM_CHANNELS*sizeof(uint8_t);
static const int    ADDR_PDM_NODEID        = ADDR_CAN_SPEED   +    sizeof(uint8_t);
static const int    ADDR_KP_KEYNODE        = ADDR_PDM_NODEID  +    sizeof(uint8_t);
static const int    ADDR_ADS_VBATT_CALIB   = ADDR_KP_KEYNODE  +    sizeof(uint8_t);  // Battery voltage calibration

// -----------------------------------------------------------------------------
// Defaults
static float         ocThresholds[NUM_CHANNELS]        = {3,3,3,3,3,3,3,3,3,3};
static float         inrushThresholds[NUM_CHANNELS]    = {5,5,5,5,5,5,5,5,5,5};
static unsigned long inrushTimeLimits[NUM_CHANNELS]    = {1000,1000,1000,1000,1000,1000,1000,1000,1000,1000};
static float         underWarnThresholds[NUM_CHANNELS] = {0.10f,0.10f,0.10f,0.10f,0.10f,0.10f,0.10f,0.10f,0.10f,0.10f};
static float         tempWarnThreshold      = 70.0f;
static float         tempTripThreshold      = 85.0f;

static OutputMode    outputMode[NUM_CHANNELS]    = {
  MODE_LATCH,MODE_LATCH,MODE_LATCH,MODE_LATCH,MODE_LATCH,MODE_LATCH,MODE_LATCH,MODE_LATCH,MODE_LATCH,MODE_LATCH
};
static uint8_t       outputGroup[NUM_CHANNELS]   = {1,2,3,4,5,6,7,8,9,10};

static uint16_t      canSpeedKbps     = 1000;
static uint8_t       pdmNodeID        = 0x15;
static uint8_t       keypadNodeID     = 0x15;
uint16_t PDMManager::digitalOutCobId = 0x680;

// -----------------------------------------------------------------------------
// ADC scaling - Arduino Uno R4 Minima
static const float   voltageReference = 5.0f;   // Arduino Uno R4 Minima uses 5V reference for most analog pins
static const int     analogResolution = 1023;   // Arduino ADC legacy scale (10-bit paths)
static const int     currentSenseAdcMax = 16383; // Direct FSP ADC is configured for 14-bit
static const float   ris              = 1000.0f;
static const float   kILIS            = 8200.0f; // Current sensor gain factor for DEV1.3 board (BTS443P)

// -----------------------------------------------------------------------------
// Dynamic
static float         overcurrentScore[NUM_CHANNELS]    = {0,0,0,0,0,0,0,0,0,0};
static float         inrushScore[NUM_CHANNELS]         = {0,0,0,0,0,0,0,0,0,0};
static unsigned long channelOnTime[NUM_CHANNELS]       = {0,0,0,0,0,0,0,0,0,0};
static bool          channelActive[NUM_CHANNELS]       = {false,false,false,false,false,false,false,false,false,false};
static bool          faultOvercurrent[NUM_CHANNELS]    = {false,false,false,false,false,false,false,false,false,false};
static bool          warningUndercurrent[NUM_CHANNELS] = {false,false,false,false,false,false,false,false,false,false};
static bool          faultThermal[NUM_CHANNELS]        = {false,false,false,false,false,false,false,false,false,false};
static bool          clearedFault[NUM_CHANNELS]        = {false,false,false,false,false,false,false,false,false,false};
static bool          resetButtonTiming[NUM_CHANNELS]   = {false,false,false,false,false,false,false,false,false,false}; // Flag to reset button press timing when fault occurs
static LEDState      currentLEDStates[NUM_CHANNELS]    = {
  LED_STATE_OFF,LED_STATE_OFF,LED_STATE_OFF,LED_STATE_OFF,LED_STATE_OFF,LED_STATE_OFF,LED_STATE_OFF,LED_STATE_OFF,LED_STATE_OFF,LED_STATE_OFF
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
// Digital switch input pins (10 switches total)
// Mapping provided by hardware pinout:
// SW1=P408, SW2=P206, SW3=P205, SW4=P204, SW5=P302,
// SW6=P012, SW7=P104, SW8=P105, SW9=P113, SW10=P301.
static const bsp_io_port_pin_t extSwitchPins[NUM_CHANNELS] = {
  BSP_IO_PORT_04_PIN_08,
  BSP_IO_PORT_02_PIN_06,
  BSP_IO_PORT_02_PIN_05,
  BSP_IO_PORT_02_PIN_04,
  BSP_IO_PORT_03_PIN_02,
  BSP_IO_PORT_00_PIN_12,
  BSP_IO_PORT_01_PIN_04,
  BSP_IO_PORT_01_PIN_05,
  BSP_IO_PORT_01_PIN_13,
  BSP_IO_PORT_03_PIN_01
};
static const unsigned long extDebounceMs = 50;

// Power output pins (10 outputs total)
// OUT1=P409, OUT2=P410, OUT3=P411, OUT4=P402, OUT5=P401,
// OUT6=P400, OUT7=P106, OUT8=P107, OUT9=P112, OUT10=P109.
static const bsp_io_port_pin_t switchPins[NUM_CHANNELS] = {
  BSP_IO_PORT_04_PIN_09,
  BSP_IO_PORT_04_PIN_10,
  BSP_IO_PORT_04_PIN_11,
  BSP_IO_PORT_04_PIN_02,
  BSP_IO_PORT_04_PIN_01,
  BSP_IO_PORT_04_PIN_00,
  BSP_IO_PORT_01_PIN_06,
  BSP_IO_PORT_01_PIN_07,
  BSP_IO_PORT_01_PIN_12,
  BSP_IO_PORT_01_PIN_09
};

// Current sensing on MCU ADC: IS1-IS8 (raw RA4M1 channels).
// Corrected mapping:
// IS1=P000/AN00, IS2=P001/AN01, IS3=P002/AN02, IS4=P003/AN03,
// IS5=P004/AN04, IS6=P011/AN06, IS7=P014/AN09, IS8=P015/AN10.
static const bsp_io_port_pin_t currentSensePins[8] = {
  BSP_IO_PORT_00_PIN_00,  // IS1
  BSP_IO_PORT_00_PIN_01,  // IS2
  BSP_IO_PORT_00_PIN_02,  // IS3
  BSP_IO_PORT_00_PIN_03,  // IS4
  BSP_IO_PORT_00_PIN_04,  // IS5
  BSP_IO_PORT_00_PIN_11,  // IS6
  BSP_IO_PORT_00_PIN_14,  // IS7
  BSP_IO_PORT_00_PIN_15   // IS8
};

static const uint8_t currentSenseChannels[8] = {
  0, 1, 2, 3, 4, 6, 9, 10
};

static adc_instance_ctrl_t currentSenseAdcCtrl;
static adc_cfg_t currentSenseAdcCfg;
static adc_channel_cfg_t currentSenseAdcChannelCfg;
static adc_extended_cfg_t currentSenseAdcExtCfg;
static bool currentSenseAdcReady = false;

static bool initCurrentSenseADC() {
  memset(&currentSenseAdcCtrl, 0, sizeof(currentSenseAdcCtrl));
  memset(&currentSenseAdcCfg, 0, sizeof(currentSenseAdcCfg));
  memset(&currentSenseAdcChannelCfg, 0, sizeof(currentSenseAdcChannelCfg));
  memset(&currentSenseAdcExtCfg, 0, sizeof(currentSenseAdcExtCfg));

  for (uint8_t i = 0; i < 8; i++) {
    pinPeripheral(currentSensePins[i], (uint32_t)IOPORT_CFG_ANALOG_ENABLE);
  }

  currentSenseAdcCfg.unit = 0;
  currentSenseAdcCfg.mode = ADC_MODE_SINGLE_SCAN;
  currentSenseAdcCfg.resolution = ADC_RESOLUTION_14_BIT;
  currentSenseAdcCfg.alignment = ADC_ALIGNMENT_RIGHT;
  currentSenseAdcCfg.trigger = ADC_TRIGGER_SOFTWARE;
  currentSenseAdcCfg.p_callback = nullptr;
  currentSenseAdcCfg.p_context = nullptr;
  currentSenseAdcCfg.p_extend = &currentSenseAdcExtCfg;
  currentSenseAdcCfg.scan_end_irq = FSP_INVALID_VECTOR;
  currentSenseAdcCfg.scan_end_ipl = 12;
  currentSenseAdcCfg.scan_end_b_irq = FSP_INVALID_VECTOR;
  currentSenseAdcCfg.scan_end_b_ipl = 12;

  currentSenseAdcExtCfg.add_average_count = ADC_ADD_OFF;
  currentSenseAdcExtCfg.clearing = ADC_CLEAR_AFTER_READ_ON;
  currentSenseAdcExtCfg.trigger_group_b = ADC_TRIGGER_SYNC_ELC;
  currentSenseAdcExtCfg.double_trigger_mode = ADC_DOUBLE_TRIGGER_DISABLED;
  currentSenseAdcExtCfg.adc_vref_control = ADC_VREF_CONTROL_AVCC0_AVSS0;
  currentSenseAdcExtCfg.enable_adbuf = 0;
  currentSenseAdcExtCfg.window_a_irq = FSP_INVALID_VECTOR;
  currentSenseAdcExtCfg.window_a_ipl = 12;
  currentSenseAdcExtCfg.window_b_irq = FSP_INVALID_VECTOR;
  currentSenseAdcExtCfg.window_b_ipl = 12;

  currentSenseAdcChannelCfg.sample_hold_states = 24;
  currentSenseAdcChannelCfg.scan_mask = 0;
  currentSenseAdcChannelCfg.scan_mask_group_b = 0;
  currentSenseAdcChannelCfg.add_mask = 0;
  currentSenseAdcChannelCfg.p_window_cfg = nullptr;
  currentSenseAdcChannelCfg.priority_group_a = ADC_GROUP_A_PRIORITY_OFF;
  currentSenseAdcChannelCfg.sample_hold_mask = 0;

  for (uint8_t i = 0; i < 8; i++) {
    currentSenseAdcChannelCfg.scan_mask |= (1UL << currentSenseChannels[i]);
  }

  if (R_ADC_Open(&currentSenseAdcCtrl, &currentSenseAdcCfg) != FSP_SUCCESS) {
    return false;
  }
  if (R_ADC_ScanCfg(&currentSenseAdcCtrl, &currentSenseAdcChannelCfg) != FSP_SUCCESS) {
    R_ADC_Close(&currentSenseAdcCtrl);
    return false;
  }

  return true;
}

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

static bool readSwitchPinLevel(bsp_io_port_pin_t pin) {
  uint8_t port = (uint8_t)(pin >> 8);
  uint8_t bit = (uint8_t)(pin & 0xFFU);
  return R_PFS->PORT[port].PIN[bit].PmnPFS_b.PIDR ? true : false;
}

static void writeSwitchPinLevel(bsp_io_port_pin_t pin, bool high) {
  R_IOPORT_PinWrite(&g_ioport_ctrl,
                    pin,
                    high ? BSP_IO_LEVEL_HIGH : BSP_IO_LEVEL_LOW);
}

// Read digital switch inputs directly
static uint16_t getExtSwitchMask() {
  uint16_t mask = 0;
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    if (!readSwitchPinLevel(extSwitchPins[i])) {  // Inverted: LOW = pressed (button pulls to ground)
      mask |= (1 << i);
    }
  }
  return mask;
}

// Group shutdown on fault
static void shutdownGroup(uint8_t ch) {
  uint8_t grp = outputGroup[ch];
  for (uint8_t i=0;i<NUM_CHANNELS;i++){
    if (outputGroup[i] == grp) {
      channelActive[i] = false;
      faultOvercurrent[i] = true;
      writeSwitchPinLevel(switchPins[i], false);
    }
  }
}

// Latch vs momentary + grouped press helper
static void applyPress(uint8_t ch, bool pressed) {
  uint8_t grp = outputGroup[ch];
  String debugMsg = "applyPress CH" + String(ch+1) + " pressed=" + String(pressed) + " group=" + String(grp);
  LOG_STATE(debugMsg);
  
  for (uint8_t i=0;i<NUM_CHANNELS;i++){
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
  for (uint8_t i=0;i<NUM_CHANNELS;i++){
    R_IOPORT_PinCfg(&g_ioport_ctrl,
                    switchPins[i],
                    (uint32_t)(IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_LOW));
    writeSwitchPinLevel(switchPins[i], false);
    channelActive[i]=false;
    faultOvercurrent[i]=false;
    warningUndercurrent[i]=false;
    faultThermal[i]=false;
    clearedFault[i]=false;
    currentLEDStates[i]=LED_STATE_OFF;
  }
  
  // Initialize digital switch input pins (all 10) with pull-ups.
  for (uint8_t i=0;i<NUM_CHANNELS;i++){
    R_IOPORT_PinCfg(&g_ioport_ctrl,
                    extSwitchPins[i],
                    (uint32_t)(IOPORT_CFG_PORT_DIRECTION_INPUT | IOPORT_CFG_PULLUP_ENABLE));
  }
  
  // Initialize MCU ADC for IS1-IS8 (direct RA4M1 channels).
  currentSenseAdcReady = initCurrentSenseADC();
  if (currentSenseAdcReady) {
    Serial.println(F("Current-sense ADC initialized (IS1-IS8)"));
  } else {
    Serial.println(F("ERROR: Current-sense ADC initialization failed!"));
  }
  
  // Initialize ADS1115 for temp sensor, battery voltage, IS5-IS10 (6 total analog channels)
  if (ads.begin()) {
    adsInitialized = true;
    // Set gain for appropriate voltage range
    // GAIN_ONE = +/-4.096V (default), suitable for our sensors
    ads.setGain(GAIN_ONE);
    Serial.println(F("ADS1115 initialized successfully"));
  } else {
    adsInitialized = false;
    Serial.println(F("ERROR: ADS1115 initialization failed!"));
  }
  
  lastUpdate = millis();
}

void PDMManager::processExternalInputs() {
  static uint16_t  lastMask      = 0;
  static uint16_t  candidateMask = 0;
  static unsigned long changeTime = 0;
  static unsigned long pressStartExt[NUM_CHANNELS] = {0,0,0,0,0,0,0,0,0,0};
  static bool          longDone[NUM_CHANNELS]     = {false,false,false,false,false,false,false,false,false,false};
  unsigned long now = millis();

  // Check for fault-induced button timing resets
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    if (resetButtonTiming[i]) {
      pressStartExt[i] = now; // Reset press start time to now
      longDone[i] = false;    // Allow new long press detection
      resetButtonTiming[i] = false; // Clear the flag
      String resetMsg = "CH" + String(i+1) + " button timing reset due to fault";
      LOG_STATE(resetMsg);
    }
  }

  uint16_t raw = getExtSwitchMask();
  if (raw != candidateMask) {
    candidateMask = raw;
    changeTime = now;
    return;
  }
  if (now - changeTime < extDebounceMs) return;

  for (uint8_t ch=0; ch<NUM_CHANNELS; ch++) {
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
        for (uint8_t i=0;i<NUM_CHANNELS;i++){
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
  // TMP235A2DBZR: 10mV/°C, 500mV offset at 0°C on ADS1115 AIN0
  // Temperature (°C) = (Vout - 0.5V) / 0.01V
  float vT = 0.0f;
  if (adsInitialized) {
    int16_t adcValue = ads.readADC_SingleEnded(0);  // AIN0 = Temperature sensor
    vT = ads.computeVolts(adcValue);
  }
  
  // Calculate temperature from TMP235A2DBZR
  // Valid output range: 0.1V (-40°C) to 2.0V (+150°C)
  float rawTemperature;
  if (!adsInitialized || vT < 0.05f || vT > 2.1f) {
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
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    // 1) If we just cleared a fault, keep it off until next short-press
    if (clearedFault[i] && !channelActive[i]) {
      currentLEDStates[i] = LED_STATE_OFF;
      writeSwitchPinLevel(switchPins[i], false);
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
      writeSwitchPinLevel(switchPins[i], false);
      continue;
    }

    // 4) Measure load current
    int16_t rawI = 0;
    float vI = 0.0f;
    readCurrentSenseRaw(i, rawI, vI);
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
    writeSwitchPinLevel(switchPins[i], true);

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
  writeSwitchPinLevel(switchPins[ch], on);
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
      for (uint8_t i=0;i<NUM_CHANNELS;i++){
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
      for (uint8_t i=0;i<NUM_CHANNELS;i++){
        if (getOutputGroup(i)==grp) {
          faultOvercurrent[i]=false;
          faultThermal[i]=false;
          clearedFault[i]=true;
        }
      }
    } else {
      if (getOutputMode(ch)==MODE_LATCH) {
        bool groupFault=false;
        for (uint8_t i=0;i<NUM_CHANNELS;i++){
          if (getOutputGroup(i)==grp &&
            (faultOvercurrent[i]||faultThermal[i])) {
            groupFault=true; break;
          }
        }
        if (!groupFault) {
          LOG_INPUT(F("→ SHORT PRESS, toggling group"));
          // toggle entire group
          bool anyOn=false;
          for (uint8_t i=0;i<NUM_CHANNELS;i++){
            if (getOutputGroup(i)==grp && channelActive[i]) {
              anyOn=true; break;
            }
          }
          for (uint8_t i=0;i<NUM_CHANNELS;i++){
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
      for (uint8_t i=0;i<NUM_CHANNELS;i++){
        if (getOutputGroup(i)==grp) setChannel(i,false);
      }
    }
  }

  lastState[ch]=pressed;
}

void PDMManager::getLEDStates(LEDState s[NUM_CHANNELS]) {
  for (uint8_t i=0;i<NUM_CHANNELS;i++) s[i]=currentLEDStates[i];
}

bool PDMManager::isChannelActive(uint8_t ch) {
  if (ch >= NUM_CHANNELS) return false;
  return channelActive[ch];
}

void PDMManager::saveConfig() {
  EEPROM.put(ADDR_MAGIC, EEPROM_MAGIC);
  for (uint8_t i=0;i<NUM_CHANNELS;i++){
    EEPROM.put(ADDR_OC_ARRAY         + i*sizeof(float),          ocThresholds[i]);
    EEPROM.put(ADDR_INRUSH_ARRAY     + i*sizeof(float),      inrushThresholds[i]);
    EEPROM.put(ADDR_INRUSHTIME_ARRAY + i*sizeof(unsigned long), inrushTimeLimits[i]);
    EEPROM.put(ADDR_UNDERWARN_ARRAY  + i*sizeof(float),    underWarnThresholds[i]);
  }
  EEPROM.put(ADDR_TEMPWARN, tempWarnThreshold);
  EEPROM.put(ADDR_TEMPTTRIP,tempTripThreshold);
  for (uint8_t i=0;i<NUM_CHANNELS;i++){
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
    for (uint8_t i=0;i<NUM_CHANNELS;i++){
      EEPROM.get(ADDR_OC_ARRAY         + i*sizeof(float),          ocThresholds[i]);
      EEPROM.get(ADDR_INRUSH_ARRAY     + i*sizeof(float),      inrushThresholds[i]);
      EEPROM.get(ADDR_INRUSHTIME_ARRAY + i*sizeof(unsigned long), inrushTimeLimits[i]);
      EEPROM.get(ADDR_UNDERWARN_ARRAY  + i*sizeof(float),    underWarnThresholds[i]);
    }
    EEPROM.get(ADDR_TEMPWARN,  tempWarnThreshold);
    EEPROM.get(ADDR_TEMPTTRIP, tempTripThreshold);
    for (uint8_t i=0;i<NUM_CHANNELS;i++){
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
  if (!adsInitialized) return 0.0f;
  
  // Battery voltage on ADS1115 AIN1 via 22k/5k divider
  // Divider ratio = (22k + 5k) / 5k = 5.4
  int16_t adcValue = ads.readADC_SingleEnded(1);  // AIN1 = Battery voltage
  float v = ads.computeVolts(adcValue);
  return v * 5.4f * vbattCalibration;  // Apply divider ratio and calibration
}

bool PDMManager::isADSReady() {
  return adsInitialized;
}

bool PDMManager::readSwitchInputRaw(uint8_t ch, bool& pressed) {
  if (ch >= NUM_CHANNELS) {
    pressed = false;
    return false;
  }

  pressed = !readSwitchPinLevel(extSwitchPins[ch]);
  return true;
}

uint16_t PDMManager::readSwitchInputMaskRaw() {
  return getExtSwitchMask();
}

bool PDMManager::readCurrentSenseRaw(uint8_t ch, int16_t& raw, float& volts) {
  if (ch < 8) {
    if (!currentSenseAdcReady) {
      raw = 0;
      volts = 0.0f;
      return false;
    }

    if (R_ADC_ScanStart(&currentSenseAdcCtrl) != FSP_SUCCESS) {
      raw = 0;
      volts = 0.0f;
      return false;
    }

    adc_status_t status;
    status.state = ADC_STATE_SCAN_IN_PROGRESS;
    uint16_t timeout = 3000;
    while (status.state == ADC_STATE_SCAN_IN_PROGRESS && timeout--) {
      R_ADC_StatusGet(&currentSenseAdcCtrl, &status);
    }

    if (status.state == ADC_STATE_SCAN_IN_PROGRESS) {
      raw = 0;
      volts = 0.0f;
      return false;
    }

    uint16_t sample = 0;
    if (R_ADC_Read(&currentSenseAdcCtrl, (adc_channel_t)currentSenseChannels[ch], &sample) != FSP_SUCCESS) {
      raw = 0;
      volts = 0.0f;
      return false;
    }

    raw = (int16_t)sample;
    volts = (float)sample / (float)currentSenseAdcMax * voltageReference;
    return true;
  }

  if (ch < 10 && adsInitialized) {
    raw = ads.readADC_SingleEnded(ch - 6);
    // Single-ended channels should not be negative; retry once then clamp.
    if (raw < 0) {
      raw = ads.readADC_SingleEnded(ch - 6);
      if (raw < 0) {
        raw = 0;
      }
    }
    volts = ads.computeVolts(raw);
    return true;
  }

  raw = 0;
  volts = 0.0f;
  return false;
}

bool PDMManager::readADSRaw(uint8_t channel, int16_t& raw, float& volts) {
  if (!adsInitialized || channel > 3) {
    raw = 0;
    volts = 0.0f;
    return false;
  }

  raw = ads.readADC_SingleEnded(channel);
  // Single-ended ADS reads should not go negative; guard against noise/transients.
  if (raw < 0) {
    raw = ads.readADC_SingleEnded(channel);
    if (raw < 0) {
      raw = 0;
    }
  }
  volts = ads.computeVolts(raw);
  return true;
}

bool PDMManager::readTemperatureRaw(int16_t& raw, float& volts, float& celsius) {
  celsius = -999.0f;

  if (!readADSRaw(0, raw, volts)) {
    return false;
  }

  // TMP235A2DBZR: T(°C) = (Vout - 0.5V) / 0.01V
  celsius = (volts - 0.5f) / 0.01f;
  return true;
}

float PDMManager::getChannelCurrent(uint8_t ch) {
  if (ch >= NUM_CHANNELS) return 0.0f;
  
  // Channels 0-7 use the MCU's raw ADC-capable ports.
  // Channels 8-9 use ADS1115 AIN2-AIN3.
  
  int16_t raw = 0;
  float volts = 0.0f;
  if (!readCurrentSenseRaw(ch, raw, volts)) {
    return 0.0f;
  }

  return volts / ris * kILIS;
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
  for (uint8_t i=0;i<NUM_CHANNELS;i++){
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