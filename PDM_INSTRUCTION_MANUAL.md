# PDM (Power Distribution Module) System - Complete Instruction Manual

## Table of Contents
1. [System Overview](#system-overview)
2. [Hardware Configuration](#hardware-configuration)
3. [Software Architecture](#software-architecture)
4. [Installation and Setup](#installation-and-setup)
5. [Operation Modes](#operation-modes)
6. [LED Status Indicators](#led-status-indicators)
7. [CAN Bus Integration](#can-bus-integration)
8. [Protection Systems](#protection-systems)
9. [Configuration Commands](#configuration-commands)
10. [Troubleshooting](#troubleshooting)
11. [Technical Specifications](#technical-specifications)

---

## System Overview

### What is this PDM System?

This is a sophisticated 4-channel Power Distribution Module designed for motorsport applications, built on the Arduino Uno R4 Minima platform. It provides intelligent power switching with comprehensive protection, monitoring, and remote control capabilities.

### Key Features

- **4 Independent Output Channels** with configurable current limits
- **Triple Input Control Methods:**
  - Local digital pushbuttons (D0-D3)
  - CAN bus keypad integration
  - CAN bus digital output control
- **Comprehensive Protection:**
  - Overcurrent protection with I²t fusing
  - Inrush current limiting
  - Thermal protection with rate limiting
  - Undercurrent warning detection
  - Hardware watchdog timer (1 second timeout)
- **Visual Status Feedback:**
  - Dual 8-pixel NeoPixel LED strips
  - CAN keypad LED control
- **Advanced Monitoring:**
  - Real-time current measurement
  - Temperature monitoring with sensor validation
  - Battery voltage monitoring
  - CAN bus health monitoring
- **Flexible Configuration:**
  - Channel grouping for synchronized operation
  - Latch vs momentary modes
  - Configurable thresholds
  - EEPROM parameter storage with CRC-16 validation

---

## Hardware Configuration

### Arduino Uno R4 Minima Pin Assignments

#### Digital Pins
- **D0-D3**: External switch inputs (with internal pull-ups)
- **D5, D6, D9, D10**: Output channel control (drives switching circuits)
- **D7**: NeoPixel Strip 1 data line
- **D8**: NeoPixel Strip 2 data line

#### Analog Pins
- **A0-A3**: Current sensing inputs for channels 1-4
- **A4**: Temperature sensor (LM335) input
- **A5**: Battery voltage sensing input

#### Communication
- **Built-in CAN controller**: CAN bus communication at 125k-1000k bps
- **USB Serial**: Configuration and monitoring interface at 115200 baud

### External Components Required

#### Current Sensing
- **Current Sense Resistor (Ris)**: 1000Ω (1kΩ)
- **Current Amplification Factor (kILIS)**: 16000 (configured in software)
- Each channel uses dedicated current sensing circuitry

#### Temperature Monitoring
- **TMP235A2DBZR Temperature Sensor**: Connected to A4
  - Output: 10mV/°C with 500mV offset at 0°C
  - Formula: Temperature (°C) = (Vout - 0.5V) / 0.01V
  - Normal room temperature (25°C) = 0.75V output
  - Sensor error detected when voltage < 0.05V or > 2.1V

#### Voltage Monitoring
- **Battery Voltage Divider**: 15kΩ + 5kΩ = 4:1 ratio on A5
  - Allows monitoring up to 20V battery systems safely

#### Power Switching
- **High-side switches**: 4x Infineon BTS443P Smart Power Switches
  - Control pins: D5, D6, D9, D10
  - Current sense outputs: A0, A1, A2, A3
- **Protection circuits**: External fuses/breakers recommended as backup

##### BTS443P Specifications
The BTS443P is an intelligent high-side power switch with integrated protection and diagnostic features:

**Electrical Ratings:**
- **Nominal Current**: 3.5A continuous at 25°C (per channel)
- **Maximum Current**: 8A intermittent (limited by thermal and overcurrent protection)
- **Operating Voltage**: 5.5V to 28V (automotive battery range)
- **On-Resistance (RON)**: ~65mΩ typical at 25°C
- **Current Sense Ratio (kILIS)**: Nominal 8500:1 (calibrated to 8200:1 for DEV1.3 hardware)

**Integrated Protection Features:**
- **Overcurrent Protection**: Automatic current limitation with configurable trip point
- **Thermal Shutdown**: Self-protection at ~150°C junction temperature
- **Overvoltage Protection**: Clamps voltage spikes up to 42V
- **Short Circuit Protection**: Fast shutdown on hard short to ground
- **Reverse Battery Protection**: Prevents damage from reverse polarity
- **ESD Protection**: ±4kV human body model

**Diagnostic Features:**
- **Current Sensing**: Proportional analog output (ILIS pin) for real-time monitoring
- **Open Load Detection**: Can detect disconnected loads via current sensing
- **Status Reporting**: Fault conditions reported via current sense output

**Thermal Characteristics:**
- **Junction Temperature Range**: -40°C to +150°C operating
- **Thermal Resistance**: ~60°C/W junction-to-case (TO-252 package)
- **Derating**: ~50mA/°C above 25°C ambient

**Important Operating Notes:**
- Each channel rated for 3.5A continuous with adequate cooling
- Maximum 5A recommended per channel for this PDM design
- Inrush current capability up to 8A for short durations (<2 seconds)
- Internal current limiting prevents catastrophic failure
- Thermal protection may reduce current at elevated temperatures
- Current sense output requires ADC filtering for accurate measurement

#### Visual Feedback
- **2x NeoPixel strips**: 8 pixels each, mirrored display
  - Type: WS2812B or compatible (NEO_GRB + NEO_KHZ800)
  - 5V LEDs only

---

## Software Architecture

### Main Components

#### 1. PDMManager (PDMManager.h/.cpp)
**Core Power Management System**
- Manages all 4 output channels
- Implements protection algorithms
- Handles channel grouping and modes
- Processes external input switches
- Stores/loads configuration from EEPROM

#### 2. CANHandler (CANHandler.h/.cpp)
**CAN Bus Communication Manager**
- Handles keypad integration
- Processes digital output commands
- Sends telemetry data
- Manages heartbeat monitoring
- Implements smart watchdog protection

#### 3. UARTHandler (UARTHandler.h/.cpp)
**Serial Command Line Interface**
- Processes configuration commands
- Provides system status reporting
- Handles parameter adjustment
- Offers comprehensive help system

#### 4. Main Application (main.ino)
**System Coordinator**
- Initializes all subsystems
- Manages main control loop
- Updates NeoPixel displays
- Coordinates component communication

### Data Flow Architecture

```
[Physical Inputs] → [PDMManager] → [Outputs]
       ↓                ↓            ↑
[CANHandler] ←→ [Main Loop] → [NeoPixels]
       ↓                ↓
[CAN Bus] ←→ [UARTHandler] ← [Serial Commands]
```

---

## Installation and Setup

### 1. Hardware Assembly

#### Power Connections
1. Connect 5V supply to Arduino Uno R4 Minima
2. Connect battery monitoring circuit to A5 (15kΩ/5kΩ divider)
3. Install LM335 temperature sensor on A4
4. Wire current sensing circuits to A0-A3

#### Control Connections
1. Install pushbuttons on D0-D3 (normally open, pulled to ground when pressed)
2. Connect output switching circuits to D5, D6, D9, D10
3. Install NeoPixel strips on D7 and D8

#### CAN Bus Connection
1. Connect CAN_H and CAN_L to vehicle CAN bus
2. Ensure proper 120Ω termination if end node
3. Verify CAN bus voltage levels (2.5V nominal, 1.5V-3.5V differential)

### 2. Software Installation

#### Using PlatformIO (Recommended)
```bash
# Clone or copy project files
cd 250823-150829-uno_r4_minima

# Install dependencies (automatic with PlatformIO)
# - Adafruit NeoPixel library
# - Built-in Arduino CAN library

# Build and upload
pio run --target upload
```

#### Library Dependencies
- **Adafruit NeoPixel**: v1.11.0 or later
- **Arduino CAN**: Built-in with Uno R4 Minima
- **EEPROM**: Built-in Arduino library

### 3. Initial Configuration

#### Connect via Serial Terminal
- **Baud Rate**: 115200
- **Data**: 8 bits, No parity, 1 stop bit
- **Flow Control**: None

#### Default Settings (Fresh Installation)
```
Channel Configuration:
- Overcurrent Threshold: 3A (all channels)
- Inrush Threshold: 5A (all channels)
- Inrush Time Limit: 1000ms (all channels)
- Undercurrent Warning: 0.1A (all channels)
- Operating Mode: LATCH (all channels)
- Channel Groups: 1, 2, 3, 4 (individual)

Temperature Thresholds:
- Warning: 70°C
- Trip: 85°C

CAN Configuration:
- Speed: 1000 kbps
- PDM Node ID: 0x15
- Keypad Node ID: 0x15
- Digital Output ID: 0x680
```

#### First-Time Setup Commands
```
HELP                    # Display all available commands
STATUS                  # Check system status
SHOW                    # Display current configuration
SAVE                    # Save configuration to EEPROM
```

---

## Operation Modes

### Channel Operating Modes

#### 1. LATCH Mode (Default)
**Behavior**: Toggle on/off with each button press
- **Short Press**: Toggles entire group on/off
- **Long Press (1+ seconds)**: Clears faults and turns group off
- **Fault Condition**: Ignores button presses until fault cleared

**Configuration**:
```
MODE 1 LATCH            # Set channel 1 to latch mode
MODE 2 LATCH            # Set channel 2 to latch mode
```

#### 2. MOMENTARY Mode
**Behavior**: On only while button is pressed
- **Press and Hold**: Channel(s) turn on
- **Release**: Channel(s) turn off
- **Long Press (1+ seconds)**: Clears faults

**Configuration**:
```
MODE 1 MOMENTARY        # Set channel 1 to momentary mode
MODE 2 MOMENTARY        # Set channel 2 to momentary mode
```

### Channel Grouping System

Channels can be grouped together for synchronized operation:

#### Individual Control (Default)
```
GROUP 1 1               # Channel 1 in group 1
GROUP 2 2               # Channel 2 in group 2
GROUP 3 3               # Channel 3 in group 3
GROUP 4 4               # Channel 4 in group 4
```

#### Example Group Configurations
```
# Headlight group (channels 1 & 2)
GROUP 1 1
GROUP 2 1

# Auxiliary group (channels 3 & 4)
GROUP 3 2
GROUP 4 2
```

**Group Behavior**:
- Pressing any button in a group affects all channels in that group
- Fault in one channel affects entire group
- Long press on any group member clears faults for entire group

### Input Priority System

The system intelligently manages multiple input sources:

#### 1. Digital Buttons (Highest Priority)
- **Pins**: D0-D3 (active low with internal pull-ups)
- **Debounce**: 50ms
- **Override**: Takes precedence over CAN inputs when pressed

#### 2. CAN Keypad
- **Protocol**: CANopen-style PDO messages
- **Node ID**: Configurable (default 0x15)
- **Heartbeat**: Required for operation (1.5s timeout)

#### 3. CAN Digital Output
- **Message ID**: Configurable (default 0x680)
- **Protocol**: 8-byte message with channel states
- **Timeout**: 2s (outputs turn off if no message received)

---

## LED Status Indicators

### NeoPixel Display System

The system uses dual 8-pixel NeoPixel strips (mirrored) for comprehensive status indication:

#### Pixel Assignments
- **Pixel 0**: Power Indicator (always solid green when system running)
- **Pixel 1**: Temperature Status
- **Pixel 2**: CAN Communication Status
- **Pixel 3**: Reserved (always off)
- **Pixels 4-7**: Channel 1-4 Status

#### Color Coding System

##### Temperature Indicator (Pixel 1)
- **🔵 Blue**: < 20°C (Cold)
- **🟢 Green**: 20°C - 70°C (Normal)
- **🟠 Orange**: 70°C - 85°C (Warning)
- **🔴 Red**: > 85°C (Critical)
- **🟣 Violet**: Sensor Error

##### CAN Status (Pixel 2)
- **⚪ White**: Normal operation
- **🟡 Yellow**: CAN timeout/error
- **🟣 Violet**: Digital output watchdog triggered

##### Channel Status (Pixels 4-7)
- **⚫ Off**: Channel disabled
- **🟢 Green**: Normal operation
- **🔵 Blue**: Undercurrent warning
- **🟠 Amber**: Over threshold but not faulted
- **🔴 Red**: Overcurrent fault
- **🔴 Flashing Red**: Thermal fault (1Hz blink)

### CAN Keypad LED Control

The system sends LED status to external CAN keypads:

#### LED Status Messages
- **Message ID**: 0x200 + Keypad Node ID
- **Frequency**: 10Hz
- **Data**: RGB color information for each channel

#### Blink Control Messages
- **Message ID**: 0x300 + Keypad Node ID
- **Purpose**: Controls flashing patterns
- **Trigger**: Sent when any channel needs flashing indication

---

## CAN Bus Integration

### Supported CAN Protocols

#### 1. CANopen-Style Keypad Integration

##### Keypad → PDM Messages
```
Boot-up Notification:    0x700 + NodeID, Data: [0x00]
Heartbeat:               0x700 + NodeID, Data: [0x05]
Button State PDO:        0x180 + NodeID, Data: [button_mask]
```

##### PDM → Keypad Messages
```
Start Command:           0x000, Data: [0x01, NodeID]
Backlight Setting:       0x500 + NodeID, Data: [0x0C, 0x07, ...]
Heartbeat Enable:        0x600 + NodeID, Data: [0x2B,0x17,0x10,0x00,0xF4,0x01,0x00,0x00]
LED Status:             0x200 + NodeID, Data: [R, G, B, ...]
LED Blink:              0x300 + NodeID, Data: [blink_mask, ...]
```

#### 2. Digital Output Control

##### Message Format
- **ID**: Configurable (default 0x680)
- **Length**: 8 bytes
- **Data Format**: [CH1_state, 0, CH2_state, 0, CH3_state, 0, CH4_state, 0]
- **Channel State**: 0x01 = ON, 0x00 = OFF

##### Behavior
- **Rising Edge**: Turns channel ON (unless fault-locked)
- **Falling Edge**: Turns channel OFF and clears fault lock
- **Fault Handling**: Channel becomes locked if overcurrent occurs
- **Timeout**: 2s (all channels turn OFF if no message received)

#### 3. Telemetry Broadcasting

##### Message Details
- **ID**: 0x380 + PDM Node ID
- **Frequency**: 4Hz (250ms interval)
- **Length**: 8 bytes

##### Data Format
```
Byte 0-3: Channel currents (0.2A per bit, 0-51A range)
Byte 4:   Board temperature (1°C per bit, 0-255°C)
Byte 5:   Fault flags (bits 0-3: undercurrent, bits 4-7: overcurrent)
Byte 6-7: Battery voltage (0.001V per bit, little-endian, 0-65.535V)
```

### CAN Configuration

#### Speed Settings
```
CANSPEED 125            # 125 kbps
CANSPEED 250            # 250 kbps
CANSPEED 500            # 500 kbps
CANSPEED 1000           # 1000 kbps (default)
```

#### Node ID Configuration
```
NODEID PDM 0x15         # Set PDM node ID (hex)
NODEID KEYPAD 0x15      # Set keypad node ID (hex)
DIGOUT 0x680           # Set digital output message ID (hex)
```

### Smart Watchdog System

The system implements intelligent watchdog monitoring based on the last input source:

#### Digital Input Mode
- **No watchdog**: Local inputs are always available
- **Behavior**: Immediate response to button presses

#### CAN Keypad Mode
- **Heartbeat monitoring**: 1.5s timeout
- **Failure action**: All outputs turn OFF
- **Recovery**: Automatic when heartbeat resumes

#### CAN Digital Output Mode
- **Message timeout**: 2s
- **Failure action**: All outputs turn OFF, state reset
- **Recovery**: New ON commands accepted after timeout

---

## Protection Systems

### 1. Overcurrent Protection (I²t Fusing)

#### Algorithm Description
The system implements a sophisticated I²t (current-squared-time) fusing algorithm that mimics the behavior of traditional fuses:

**Formula**: `score += dt × (excess_ratio)²`
- Where `excess_ratio = (measured_current / threshold) - 1`
- Fault triggers when `score ≥ 1.0`

#### Dual-Stage Protection

##### Stage 1: Inrush Protection
- **Active Period**: First 1000ms after channel turn-on (configurable)
- **Purpose**: Handles motor starting currents, capacitive loads
- **Threshold**: Higher limit (default 5A)
- **Reset**: Score resets to 0 when current drops below threshold

##### Stage 2: Normal Operation Protection
- **Active Period**: After inrush window expires
- **Purpose**: Continuous overcurrent protection
- **Threshold**: Lower limit (default 3A)
- **Behavior**: Accumulated thermal damage model

#### Configuration Commands
```
OC 1 2.5                # Set channel 1 overcurrent to 2.5A
INRUSH 1 8.0            # Set channel 1 inrush to 8.0A
INRUSHTIME 1 1500       # Set channel 1 inrush window to 1500ms
```

### 2. Thermal Protection

#### Temperature Monitoring
- **Sensor**: TMP235A2DBZR precision temperature sensor
- **Output**: 10mV/°C with 500mV offset at 0°C
- **Location**: On-board near switching components
- **Resolution**: 0.1°C
- **Range**: -40°C to +150°C

#### Protection Levels
```
Temperature Warning:     70°C (configurable)
Temperature Trip:        85°C (configurable)
Sensor Error:           Analog reading ≥ 1000 (pegged high)
```

#### Thermal Response
- **Warning Level**: Visual indication only
- **Trip Level**: Immediate shutdown of entire channel group
- **Sensor Error**: Immediate shutdown (assumes worst-case scenario)

#### Configuration
```
TEMPWARN 75             # Set warning threshold to 75°C
TEMPTRIP 90             # Set trip threshold to 90°C
```

### 3. Undercurrent Warning

#### Purpose
Detects failed loads, broken connections, or unexpected circuit changes.

#### Implementation
- **Continuous monitoring**: During channel ON state
- **Threshold**: Configurable per channel (default 0.1A)
- **Action**: Warning indication only (no shutdown)
- **Display**: Blue LED indication

#### Configuration
```
UNDERWARN 1 0.05        # Set channel 1 undercurrent warning to 0.05A
UNDERWARN 2 0.2         # Set channel 2 undercurrent warning to 0.2A
```

### 4. Fault Recovery System

#### Automatic Recovery
- **Thermal faults**: Auto-clear when temperature drops below warning level
- **Overcurrent faults**: Require manual intervention
- **System lockup**: Hardware watchdog automatically resets system after 1 second

#### Manual Fault Clearing
- **Long Press**: Hold any button in affected group for 1+ seconds
- **Effect**: Clears all faults for entire group, turns outputs OFF
- **LED Indication**: Special cleared state until next activation

#### Fault Locking (CAN Digital Output Mode)
- **Behavior**: Channel locks OFF after overcurrent fault
- **Purpose**: Prevents rapid cycling that could damage components
- **Recovery**: Falling edge on CAN digital output message clears lock

### 5. Hardware Watchdog Timer

#### System Reliability Protection
- **Timeout**: 1 second
- **Purpose**: Automatically resets the system if software hangs or locks up
- **Monitoring**: Detects complete CPU failure, stuck loops, or hung peripherals
- **Recovery**: Full system restart from clean state

#### Watchdog Behavior
- **Normal Operation**: Automatically refreshed each loop iteration
- **System Hang**: If no refresh for 1 second, chip automatically resets
- **After Reset**: System reinitializes with last saved EEPROM configuration
- **Safety**: All outputs turn OFF during reset, preventing dangerous states

---

## Configuration Commands

### Serial Command Interface

#### Connection Settings
- **Baud Rate**: 115200
- **Data Format**: 8N1 (8 data bits, no parity, 1 stop bit)
- **Line Ending**: Newline (\n)
- **Timeout**: 2 seconds for serial connection detection

### Command Reference

#### Current Protection Commands
```
OC <channel> <amps>           # Set overcurrent threshold
  Example: OC 1 2.5           # Channel 1 overcurrent = 2.5A

INRUSH <channel> <amps>       # Set inrush current threshold
  Example: INRUSH 1 8.0       # Channel 1 inrush = 8.0A

INRUSHTIME <channel> <ms>     # Set inrush time window
  Example: INRUSHTIME 1 1500  # Channel 1 inrush window = 1.5s

UNDERWARN <channel> <amps>    # Set undercurrent warning
  Example: UNDERWARN 1 0.1    # Channel 1 undercurrent = 0.1A
```

#### Temperature Commands
```
TEMPWARN <celsius>            # Set temperature warning threshold
  Example: TEMPWARN 75        # Warning at 75°C

TEMPTRIP <celsius>            # Set temperature trip threshold
  Example: TEMPTRIP 90        # Trip at 90°C
```

#### Channel Configuration Commands
```
MODE <channel> <type>         # Set channel operating mode
  Example: MODE 1 LATCH       # Channel 1 = latch mode
  Example: MODE 2 MOMENTARY   # Channel 2 = momentary mode

GROUP <channel> <group>       # Set channel group
  Example: GROUP 1 1          # Channel 1 in group 1
  Example: GROUP 2 1          # Channel 2 in group 1 (same group)
```

#### CAN Configuration Commands
```
CANSPEED <kbps>              # Set CAN bus speed
  Example: CANSPEED 500       # 500 kbps
  Valid: 125, 250, 500, 1000

NODEID <type> <id>           # Set node IDs
  Example: NODEID PDM 0x15    # PDM node ID = 0x15
  Example: NODEID KEYPAD 0x20 # Keypad node ID = 0x20

DIGOUT <id>                  # Set digital output CAN ID
  Example: DIGOUT 0x680       # Listen on ID 0x680
  Example: DIGOUT 1664        # Same ID in decimal
```

#### System Commands
```
SHOW                         # Display complete configuration
PRINT                        # Same as SHOW
STATUS                       # Display comprehensive system status
SAVE                         # Save configuration to EEPROM with CRC-16 checksum
LOAD                         # Load configuration from EEPROM with CRC validation
HELP                         # Display command help
?                           # Same as HELP
```

### Configuration Storage and CRC Validation

#### EEPROM CRC Protection
The system uses CRC-16 (IBM polynomial) to validate configuration data integrity:

- **On SAVE**: Calculates and stores CRC checksum with configuration
  ```
  > SAVE
  OK: Configuration saved (CRC=0x1A2B)
  ```

- **On LOAD/Startup**: Verifies stored CRC matches calculated CRC
  ```
  OK: Configuration loaded (CRC=0x1A2B)    # Valid configuration
  WARN: Config CRC mismatch! ...           # Corrupted data detected
  ```

#### Benefits
- **Corruption Detection**: Identifies EEPROM damage from power loss, bit flips, or wear
- **Data Integrity**: Ensures loaded configuration is valid before use
- **Safe Fallback**: System warns but continues with loaded data for diagnosis

### Configuration Examples

#### Fuel Pump Application
```
OC 1 3.0                   # 3A continuous current
INRUSH 1 5.0              # 5A starting current
INRUSHTIME 1 2000          # 2 second start window
UNDERWARN 1 0.5            # 0.5A minimum load
MODE 1 LATCH               # Toggle operation
GROUP 1 1                  # Individual control
```

#### LED Lighting Array
```
OC 2 1.0                   # 1A continuous current
INRUSH 2 2.0              # 2A inrush for LED drivers
INRUSHTIME 2 500          # 0.5 second inrush window
UNDERWARN 2 0.05          # 50mA minimum load
MODE 2 MOMENTARY          # Momentary operation
GROUP 2 2                 # Individual control
```

#### Grouped Auxiliary Lights
```
# Configure both channels identically
OC 1 4.0; OC 2 4.0        # 4A each channel
INRUSH 1 6.0; INRUSH 2 6.0  # 6A inrush each
MODE 1 LATCH; MODE 2 LATCH   # Latch mode
GROUP 1 1; GROUP 2 1         # Same group (synchronized)
```

---

## Troubleshooting

### Common Issues and Solutions

#### 1. System Not Starting

**Symptoms**: No serial output, no LED activity
**Possible Causes**:
- Power supply issues
- USB connection problems
- Arduino board failure

**Solutions**:
1. Check 5V power supply (minimum 1A capacity)
2. Verify USB cable connection
3. Try different USB port or computer
4. Check for short circuits in wiring
5. Measure voltage at Arduino 5V pin

#### 2. CAN Bus Communication Failure

**Symptoms**: Yellow CAN status LED, no keypad response
**Possible Causes**:
- Incorrect CAN wiring
- Wrong CAN speed setting
- Missing termination resistors
- CAN bus voltage issues

**Diagnostic Steps**:
```
STATUS                      # Check CAN status
CANSPEED 500               # Try different speeds
NODEID PDM 0x10           # Try different node ID
```

**Solutions**:
1. Verify CAN_H and CAN_L connections
2. Check for 120Ω termination at bus ends
3. Measure CAN bus voltage (should be ~2.5V nominal)
4. Ensure CAN speed matches network
5. Check node ID conflicts

#### 3. Temperature Sensor Issues

**Symptoms**: Violet temperature LED, thermal faults
**Possible Causes**:
- LM335 sensor failure
- Open circuit in sensor wiring
- Wrong sensor type
- Noise on analog input

**Diagnostic Commands**:
```
STATUS                      # Check temperature reading
TEMPWARN 100               # Temporarily raise threshold
TEMPTRIP 120               # Temporarily raise threshold
```

**Solutions**:
1. Check LM335 wiring (3 pins: +5V, Ground, Output to A4)
2. Measure sensor output voltage (should be ~2.98V at 25°C)
3. Replace sensor if reading is constant at 5V
4. Add 0.1µF capacitor from A4 to ground for noise filtering

#### 4. Current Sensing Problems

**Symptoms**: Incorrect current readings, false overcurrent faults
**Possible Causes**:
- Wrong current sense resistor value
- Scaling factor errors
- Noisy current measurements
- Broken current sense circuit

**Diagnostic Approach**:
```
STATUS                      # Check current readings
OC 1 50.0                  # Temporarily disable protection
UNDERWARN 1 0.001          # Set very low undercurrent threshold
```

**Solutions**:
1. Verify current sense resistor = 1000Ω (1kΩ)
2. Check scaling factors in code (ris = 1000.0, kILIS = 16000.0)
3. Measure actual current with external meter for verification
4. Add filtering capacitors to current sense inputs
5. Check for proper ground connections

#### 5. Output Switching Problems

**Symptoms**: Channels not turning on/off, erratic operation
**Possible Causes**:
- Faulty switching circuits
- Insufficient gate drive
- Ground loop issues
- Overloaded outputs

**Testing Procedure**:
1. Check output pin voltages (D5, D6, D9, D10)
2. Should read 5V when ON, 0V when OFF
3. Verify switching circuit can handle load current
4. Check for proper heat sinking on power components

#### 6. EEPROM Configuration Issues

**Symptoms**: Settings don't persist after restart, CRC mismatch warnings
**Possible Causes**:
- EEPROM write failures
- Corrupted configuration data (power loss during SAVE)
- EEPROM wear-out
- Address conflicts

**Diagnostic Messages**:
```
INFO: No saved config.                      # First boot or EEPROM cleared
OK: Configuration loaded (CRC=0x1A2B)       # Valid configuration
WARN: Config CRC mismatch! Stored=0x1A2B, Calculated=0x5678  # Corruption detected
```

**Recovery Commands**:
```
LOAD                       # Try to load saved config
SHOW                      # Check what was loaded
# If CRC mismatch, verify critical settings:
STATUS                    # Check current thresholds
# Reconfigure manually if needed
SAVE                      # Save working configuration (generates new CRC)
```

### Diagnostic Commands

#### System Health Check
```
STATUS                     # Comprehensive system status
SHOW                      # Configuration display
HELP                      # Command reference
```

#### Monitoring Loop
```
# Set up serial monitor and watch for:
# - Current readings per channel
# - Temperature values
# - Fault conditions
# - CAN message activity
```

### LED Diagnostic Patterns

#### Power-On Self Test
- **Green Power LED**: System running
- **White CAN LED**: CAN bus OK
- **Green/Blue Temp LED**: Normal temperature
- **All channels OFF**: Normal startup state

#### Fault Indication Patterns
- **Red Solid**: Overcurrent fault
- **Red Flashing**: Thermal fault
- **Blue**: Undercurrent warning
- **Violet**: Sensor error or CAN watchdog
- **Yellow**: CAN timeout

### Emergency Procedures

#### Complete System Reset
1. Power cycle the Arduino
2. Enter configuration commands:
```
LOAD                      # Load last saved config
# OR manually reconfigure:
TEMPWARN 70
TEMPTRIP 85
OC 1 3.0; OC 2 3.0; OC 3 3.0; OC 4 3.0
SAVE                      # Save new configuration
```

#### Factory Reset (if EEPROM corrupted)
1. The system will start with defaults if EEPROM magic number is wrong
2. Reconfigure all parameters manually
3. Use `SAVE` command to store new configuration

---

## Technical Specifications

### Electrical Characteristics

#### Power Requirements
- **Supply Voltage**: 5V DC ±5%
- **Supply Current**: 
  - Quiescent: ~200mA (with NeoPixels)
  - Peak: ~500mA (during CAN transmission)
- **Logic Levels**: 3.3V/5V compatible

#### Current Sensing
- **Method**: Resistive sensing with amplification
- **Sense Resistor**: 1000Ω ±1%
- **Amplification**: 8200:1 ratio (DEV1.3 calibrated)
- **Resolution**: ~0.01A (limited by 10-bit ADC)
- **Maximum Measurable**: 51A (limited by scaling factor)
- **Accuracy**: ±2% typical (calibrated for BTS443P driver)

#### Temperature Sensing
- **Sensor**: TMP235A2DBZR precision temperature sensor
- **Output**: 10mV/°C with 500mV offset at 0°C
- **Range**: -40°C to +150°C operational
- **Accuracy**: ±1.5°C from -40°C to +125°C
- **Resolution**: 0.1°C (software limited)

#### Voltage Monitoring
- **Input Range**: 0-20V (via 4:1 divider)
- **Resolution**: ~0.02V (10-bit ADC with 5V reference)
- **Accuracy**: ±2% typical

### Communication Specifications

#### CAN Bus
- **Protocol**: CAN 2.0B (29-bit extended frames supported)
- **Speeds**: 125k, 250k, 500k, 1000k bps
- **Connector**: Depends on implementation
- **Termination**: 120Ω required at bus ends
- **Cable**: Twisted pair, 120Ω characteristic impedance

#### Serial Interface
- **Protocol**: RS-232 compatible (over USB)
- **Speed**: 115200 bps
- **Format**: 8N1 (8 data, no parity, 1 stop)
- **Flow Control**: None
- **Buffer**: Arduino default (64 bytes receive)

#### Digital Inputs
- **Type**: TTL/CMOS compatible
- **Pull-up**: Internal 20kΩ typical
- **Active Level**: LOW (button connects to ground)
- **Debounce**: 50ms software debounce
- **Maximum Frequency**: ~10Hz (due to debounce)

### Performance Characteristics

#### Response Times
- **Digital Input Response**: <50ms (debounce limited)
- **CAN Message Processing**: <10ms
- **Overcurrent Detection**: 10-100ms (depends on current level)
- **Temperature Update**: 100ms typical
- **LED Update Rate**: 10Hz (100ms period)

#### Protection Thresholds (Defaults)
- **Overcurrent**: 3A continuous, 5A inrush
- **Inrush Window**: 1000ms
- **Temperature Warning**: 70°C
- **Temperature Trip**: 85°C
- **Undercurrent Warning**: 0.1A
- **CAN Timeout**: 1.5s (keypad), 2s (digital output)

#### Memory Usage
- **Flash**: ~50% of Uno R4 Minima capacity
- **RAM**: ~30% of available RAM
- **EEPROM**: ~100 bytes for configuration storage

### Environmental Specifications

#### Operating Conditions
- **Temperature**: -20°C to +70°C (based on Arduino specs)
- **Humidity**: 5% to 95% non-condensing
- **Vibration**: Typical automotive levels (with proper mounting)
- **Shock**: Handled by mechanical design

#### Storage Conditions
- **Temperature**: -40°C to +85°C
- **Humidity**: 5% to 95% non-condensing

### Certifications and Standards
- **FCC Part 15**: Compliance depends on enclosure and final implementation
- **Automotive**: Designed for automotive 12V/24V systems
- **Safety**: External fusing and protection recommended

---

## Advanced Configuration Examples

### Motorsport Auxiliary System
```
# Channel 1: Fuel pump
OC 1 3.0                  # 3A continuous
INRUSH 1 5.0              # 5A starting current
INRUSHTIME 1 2000         # 2 second start window
MODE 1 LATCH              # Toggle operation
GROUP 1 1                 # Individual control

# Channel 2: Ignition coil
OC 2 2.5                  # 2.5A continuous
INRUSH 2 4.0              # 4A starting current
MODE 2 LATCH              # Toggle operation
GROUP 2 2                 # Individual control

# Grouped dashboard lights (channels 3 & 4)
OC 3 2.0; OC 4 2.0        # 2A each light circuit
INRUSH 3 3.0; INRUSH 4 3.0  # 3A inrush each
MODE 3 LATCH; MODE 4 LATCH     # Toggle operation
GROUP 3 3; GROUP 4 3           # Same group

# Temperature protection
TEMPWARN 80               # High performance application
TEMPTRIP 95               # Higher trip point

# Fast CAN for real-time control
CANSPEED 1000             # 1 Mbps

SAVE                      # Save motorsport configuration
```

### General Automotive Application
```
# Standard automotive loads (under 5A each)
OC 1 4.0; OC 2 4.0; OC 3 4.0; OC 4 4.0  # 4A each channel
INRUSH 1 6.0; INRUSH 2 6.0; INRUSH 3 6.0; INRUSH 4 6.0  # 6A inrush

# Conservative temperature limits
TEMPWARN 65
TEMPTRIP 80

# Individual channel control
GROUP 1 1; GROUP 2 2; GROUP 3 3; GROUP 4 4
MODE 1 LATCH; MODE 2 LATCH; MODE 3 LATCH; MODE 4 LATCH

# Standard CAN speed
CANSPEED 500

SAVE
```

### Low-Power LED Application
```
# LED lighting channels
OC 1 1.0; OC 2 1.0; OC 3 1.0; OC 4 1.0      # 1A each
INRUSH 1 2.0; INRUSH 2 2.0; INRUSH 3 2.0; INRUSH 4 2.0  # 2A inrush
INRUSHTIME 1 200; INRUSHTIME 2 200; INRUSHTIME 3 200; INRUSHTIME 4 200  # 200ms

# Sensitive undercurrent detection
UNDERWARN 1 0.02; UNDERWARN 2 0.02; UNDERWARN 3 0.02; UNDERWARN 4 0.02

# Standard temperature
TEMPWARN 70
TEMPTRIP 85

SAVE
```

---

## Maintenance and Calibration

### Routine Maintenance

#### Weekly Checks
1. Verify system status: `STATUS`
2. Check temperature readings during operation
3. Monitor current readings under normal loads
4. Verify CAN communication if using external controls

#### Monthly Checks
1. Backup configuration: `SHOW` → save output to file
2. Test fault protection with known overload
3. Verify all input methods (buttons, CAN) function correctly
4. Check NeoPixel LEDs for proper color display

#### Annual Maintenance
1. Recalibrate current sensing if available calibration sources
2. Verify temperature sensor accuracy
3. Check all electrical connections for corrosion
4. Update firmware if newer versions available

### Calibration Procedures

#### Current Sensing Calibration
If you have access to precision current measurement:
1. Apply known current to each channel
2. Compare reading from `STATUS` command
3. Adjust scaling factors in code if necessary
4. Typical accuracy should be ±2%

#### Temperature Calibration
1. Use precision thermometer at known temperature
2. Compare with `STATUS` reading
3. TMP235A2DBZR should be accurate within ±1.5°C
4. Replace sensor if readings are consistently off by more than ±3°C
5. Sensor output can be verified: (Temp °C × 0.01V) + 0.5V = expected voltage

### Performance Monitoring

#### Data Logging
For long-term monitoring, consider logging:
- Current readings per channel
- Temperature trends
- Fault frequency
- CAN communication health

#### Key Performance Indicators
- **Fault Rate**: Should be <0.1% under normal operation
- **Temperature**: Should stay below warning threshold
- **Current Accuracy**: Within ±5% of external measurements
- **Communication Uptime**: >99% for CAN systems

---

## Safety Considerations

### Electrical Safety
1. **Always use external fuses** as backup protection
2. **Verify current ratings** of all switching components
3. **Proper grounding** essential for accurate current sensing
4. **Heat sinking** required for high-current applications

### Operational Safety
1. **Test all fault conditions** before deploying
2. **Verify emergency shutdown** procedures work
3. **Document all configuration changes**
4. **Train operators** on LED status meanings

### System Integration Safety
1. **CAN bus isolation** recommended in critical applications
2. **Redundant shutdown methods** for safety-critical loads
3. **Regular backup** of configuration settings
4. **Fail-safe defaults** should be safe for your application

---

## Conclusion

This PDM system provides comprehensive power distribution control with extensive protection and monitoring capabilities. The modular software architecture allows for easy customization and expansion for specific applications.

Key benefits:
- **Comprehensive Protection**: Multiple layers of current, thermal, and communication protection
- **Flexible Control**: Three independent input methods with intelligent arbitration
- **Rich Feedback**: Visual and telemetry status for complete system awareness
- **Easy Configuration**: Serial command interface with persistent storage
- **Professional Features**: CAN bus integration and real-time monitoring

For technical support or questions about specific applications, refer to the source code comments and this manual's troubleshooting section.

---

## Recent Updates (v2.0 - November 2025)

### Hardware Changes
- **Temperature Sensor**: Upgraded from LM335 to TMP235A2DBZR for improved accuracy and range
- **Current Sensing**: Calibrated kILIS constant to 8200 for DEV1.3 hardware

### Software Improvements
- **Hardware Watchdog Timer**: 1-second timeout provides automatic recovery from system lockup
- **EEPROM CRC Validation**: CRC-16 checksum protects configuration data integrity
- **Enhanced Temperature Filtering**: Rate-limited updates prevent false readings
- **Input Handler Removed**: Obsolete analog ladder code eliminated (now using D0-D3 digital inputs)
- **Code Cleanup**: Removed all backup files, debug prints, and unused modules

### Bug Fixes
- **Serial Timeout**: Set to 100ms to prevent blocking on malformed commands
- **Race Condition**: Removed duplicate UARTHandler::process() call in serialEvent()
- **Build Flag**: Removed unnecessary --allow-multiple-definition flag

### Documentation
- Complete instruction manual updated with all new features
- Pin assignments verified and documented
- Troubleshooting section expanded with CRC validation

---

**Document Version**: 2.0  
**Last Updated**: November 9, 2025  
**Compatible Firmware**: PTPD-DEV1 (November 2025)  
**Hardware Platform**: Arduino Uno R4 Minima (DEV1.3 board)  
**Hardware Revision**: DEV1.3 with TMP235A2DBZR temperature sensor  
