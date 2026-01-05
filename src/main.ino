#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <WDT.h>
#include "PDMManager.h"
#include "CANHandler.h"
#include "UARTHandler.h"
#include "Logger.h"

static unsigned long lastCANLedMs = 0;
static const unsigned long CAN_LED_PERIOD = 100;  // 10 Hz (was 67ms ≈15Hz)
LEDState keypadStates[4];
static bool firstLoop = true;

#define NEOPIXEL_PIN_1   7
#define NEOPIXEL_PIN_2   8
#define NEOPIXEL_COUNT   8
#define CAN_TERM_PIN     12  // CAN termination resistor control (HIGH=OFF, LOW=ON)

Adafruit_NeoPixel strip1(NEOPIXEL_COUNT, NEOPIXEL_PIN_1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2(NEOPIXEL_COUNT, NEOPIXEL_PIN_2, NEO_GRB + NEO_KHZ800);

// LED brightness (10% of max)
#define LED_BRIGHTNESS 15  // 0-255, 15 = ~10% brightness

// Helper to update both 8-pixel status displays (mirrored)
void updateNeoPixels() {
  // Pixel 0: Power indicator (always solid green)
  strip1.setPixelColor(0, strip1.Color(0,LED_BRIGHTNESS,0));
  strip2.setPixelColor(0, strip2.Color(0,LED_BRIGHTNESS,0));

  // Pixel 1: Temperature indicator
  float T = PDMManager::getLastTemperature();
  uint32_t tempColor;
  if (PDMManager::isTempSensorError()) {
    tempColor = strip1.Color(LED_BRIGHTNESS,0,LED_BRIGHTNESS);        // violet
  } else if (T < 20.0f) {
    tempColor = strip1.Color(0,0,LED_BRIGHTNESS);          // blue
  } else if (T < PDMManager::getTempWarnThreshold()) {
    tempColor = strip1.Color(0,LED_BRIGHTNESS,0);          // green
  } else if (T < PDMManager::getTempTripThreshold()) {
    tempColor = strip1.Color(LED_BRIGHTNESS,LED_BRIGHTNESS/2,0);         // orange
  } else {
    tempColor = strip1.Color(LED_BRIGHTNESS,0,0);          // red
  }
  strip1.setPixelColor(1, tempColor);
  strip2.setPixelColor(1, tempColor);

  // Pixel 2: CAN status (white=OK, violet=DIGOUT watchdog, yellow=CAN timeout)
  bool canOK = CANHandler::isCANOK();
  bool digOutWatchdog = CANHandler::isDigitalOutputWatchdogTriggered();
  uint32_t canColor;
  
  if (digOutWatchdog) {
    canColor = strip1.Color(LED_BRIGHTNESS,0,LED_BRIGHTNESS);     // violet for CAN DIGOUT watchdog
  } else if (canOK) {
    canColor = strip1.Color(LED_BRIGHTNESS,LED_BRIGHTNESS,LED_BRIGHTNESS);   // white for normal operation
  } else {
    canColor = strip1.Color(LED_BRIGHTNESS,LED_BRIGHTNESS,0);     // yellow for CAN timeout
  }
  
  strip1.setPixelColor(2, canColor);
  strip2.setPixelColor(2, canColor);

  // Pixel 3: reserved (off)
  strip1.setPixelColor(3, strip1.Color(0,0,0));
  strip2.setPixelColor(3, strip2.Color(0,0,0));

  // Pixels 4–7: channel 1–4 states
  LEDState states[4];
  PDMManager::getLEDStates(states);
  for (uint8_t ch = 0; ch < 4; ch++) {
    uint8_t pix = 4 + ch;
    uint32_t col = 0;
    switch (states[ch]) {
      case LED_STATE_OFF:       col = strip1.Color(0,0,0);     break;
      case LED_STATE_GREEN:     col = strip1.Color(0,LED_BRIGHTNESS,0);   break;
      case LED_STATE_BLUE:      col = strip1.Color(0,0,LED_BRIGHTNESS);   break;
      case LED_STATE_AMBER:     col = strip1.Color(LED_BRIGHTNESS,LED_BRIGHTNESS/2,0);  break;
      case LED_STATE_RED:       col = strip1.Color(LED_BRIGHTNESS,0,0);   break;
      case LED_STATE_RED_FLASH:
        // simple ~1 Hz blink
        col = (millis() & 0x200) ? strip1.Color(LED_BRIGHTNESS,0,0) : strip1.Color(0,0,0);
        break;
    }
    strip1.setPixelColor(pix, col);
    strip2.setPixelColor(pix, col);
  }

 static uint32_t lastNeo = 0;
if (millis() - lastNeo >= 100) {
  lastNeo = millis();
  strip1.show();
  strip2.show();
}
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(100);  // 100ms timeout for parseInt/parseFloat to prevent blocking
  
  // Initialize logging system
  Logger::init();
  
  if (Serial) {
    Serial.println(F("===== PDM System Starting ====="));
    Serial.println(F("Type HELP for CLI commands"));
  }
  PDMManager::init();
  CANHandler::begin();
  
  // Initialize CAN termination resistor with saved setting
  pinMode(CAN_TERM_PIN, OUTPUT);
  bool termEnabled = PDMManager::getCANTermEnabled();
  digitalWrite(CAN_TERM_PIN, termEnabled ? LOW : HIGH);  // LOW = termination ON
  
  strip1.begin();
  strip1.begin();
  strip1.show();
  strip2.begin();
  strip2.show();
  
  // Initialize hardware watchdog timer (1 second timeout)
  WDT.begin(1000000); // 1000000 microseconds = 1 second
  if (Serial) {
    Serial.println(F("Hardware Watchdog Timer enabled (1s timeout)"));
    Serial.println(F("===== System Ready ====="));
  }
}

void loop() {
  // Print configuration on first loop iteration (deferred from setup for faster boot)
  if (firstLoop) {
    firstLoop = false;
    if (Serial) {
      PDMManager::printConfig();
    }
  }
  
  UARTHandler::process();  // Enable UART command processing
  PDMManager::processExternalInputs();
  CANHandler::process();
  PDMManager::update();
  updateNeoPixels();
  CANHandler::sendTelemetry();
  CANHandler::checkWatchdog();
  
  // Pet the watchdog - system is running normally
  WDT.refresh();

  // every 100ms, send the LED status & any blink mask to the keypad (10Hz)
unsigned long now = millis();
if (now - lastCANLedMs >= CAN_LED_PERIOD) {
  lastCANLedMs = now;

  // 1. Grab the current LED states from PDMManager
  PDMManager::getLEDStates(keypadStates);

  // 2. Send the steady‐on colors (0x200 + NodeID)
  CANHandler::sendKeypadLEDStatus(keypadStates);

  // 3. If any channel needs flashing, send the blink mask (0x300 + NodeID)
  bool anyFlash = false;
  for (int i = 0; i < 4; ++i) {
    if (keypadStates[i] == LED_STATE_RED_FLASH) {
      anyFlash = true;
      break;
    }
  }
  if (anyFlash) {
    CANHandler::sendKeypadLEDBlinkStatus(keypadStates);
  }
}
}
