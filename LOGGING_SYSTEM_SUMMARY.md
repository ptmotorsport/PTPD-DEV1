# Serial Logging System - Implementation Summary
Co-Pilot wrote this and no checks or changes have been made yet
TODO: Read and check this documentation for factuality and appropriateness 

## Overview
A comprehensive serial logging system has been implemented with three configurable levels to control the amount of information sent to the serial terminal.

## Logging Levels

### Level 0 - NORMAL (Default)
- **Purpose**: Minimal output for production use
- **Content**: 
  - Command responses and confirmations
  - Error messages
  - System startup messages
  - Critical alerts only

### Level 1 - STATE CHANGES & INPUTS
- **Purpose**: Detailed debugging of system behavior
- **Content**: 
  - All Level 0 content PLUS:
  - `[STATE]` Channel state changes (ON/OFF, fault conditions)
  - `[STATE]` Protection system activations (overcurrent, thermal)
  - `[STATE]` Watchdog timeouts and recoveries
  - `[INPUT]` Button press/release events (digital and CAN)
  - `[INPUT]` Input mode changes and timing details

### Level 2 - FULL CAN MESSAGING
- **Purpose**: Complete system debugging including CAN bus
- **Content**: 
  - All Level 1 content PLUS:
  - `[CAN-TX]` All transmitted CAN messages with full details
  - `[CAN-RX]` All received CAN messages with full details
  - Message format: `[CAN-TX] ID:0x200 LEN:8 DATA:[0x01,0x02,0x03,...]`

## Usage Commands

### Set Logging Level
```
LOG 0          # Set to Normal mode
LOG 1          # Set to State Changes & Inputs mode  
LOG 2          # Set to Full CAN messaging mode
```

### Check Current Level
```
LOG            # Display current logging level (no parameters)
```

## Implementation Details

### New Files Added
- **`Logger.h`**: Header file with logging system interface
- **`Logger.cpp`**: Implementation of all logging functions

### Key Features
- **Memory Efficient**: Uses Flash strings (`F()` macro) where possible
- **Conditional Compilation**: Only logs when appropriate level is set
- **Formatted Output**: Consistent prefixes for different message types
- **Hex Formatting**: Proper hex display for CAN IDs and data
- **Non-intrusive**: Existing functionality unchanged, just enhanced

### Integration Points
- **Main Loop**: Logger initialization in `setup()`
- **PDMManager**: State changes, input events, fault conditions
- **CANHandler**: CAN message transmission/reception, watchdog events
- **UARTHandler**: New LOG command added to CLI

## Example Output

### Level 0 (Normal)
```
===== PDM System Starting =====
CAN Initialized at 1000 kbps
===== System Ready =====
OK: CH1 OC=2.50 A
```

### Level 1 (+ State & Input)
```
[INPUT] Ext CH1 PRESSED
[INPUT] Ext CH1 RELEASED after 150 ms
[INPUT] Ext CH1 SHORT PRESS
[STATE] Channel 1 Inrush fuse blown. Shutting down.
[INPUT] CAN CH2 PRESSED
[STATE] Watchdog: CAN keypad lost → outputs OFF
```

### Level 2 (+ CAN Messages)
```
[CAN-TX] ID:0x200 LEN:8 DATA:[0x01,0x02,0x00,0x00,0x00,0x00,0x00,0x00]
[CAN-RX] ID:0x195 LEN:1 DATA:[0x01]
[CAN-TX] ID:0x395 LEN:8 DATA:[0x0F,0x0A,0x08,0x05,0x4A,0x10,0xE8,0x03]
```

## Benefits

1. **Development**: Full visibility into system operation
2. **Debugging**: Trace issues through state changes and CAN traffic
3. **Production**: Clean output for normal operation
4. **Performance**: Minimal overhead when logging is disabled
5. **Maintenance**: Easy to identify communication and timing issues

## Backward Compatibility
- All existing functionality preserved
- Default behavior unchanged (Level 0)
- Existing serial commands work exactly as before
- No impact on system performance at default level
