# PTPD-DEV1

## PT Motorsport - Power Distribution - Development Board

Thank you for your interested in another PT Motorsport Open Source Project! We're so glad you're here and looking forward to seeing what you can produce! Make sure you tag us @ptmotorsportau to show off your creations!

This project is code to support the PTPD-DEV1 Development board operating in conjuction with an Arduino UNO R4 Minima

Hardware Kits are avaliable from PT Motorsport AU website https://www.ptmotorsport.com.au/product/pt-pd-dev1/

## Scope

The Purpose of this development board is to make an affordable hardware solution that is in feature parity with the main PTPDSS units so that the motorsport community can develop their ideas for software and firmware improvements or customisations without the need for purchasing a PTPDSS.

## Hardware

The PTPD-DEV1 Hardware is in an Arduino Uno Shield format for ease of use, this shield is compatible with the Ardunino Uno R4 Minima ONLY.
Schematics are delivered to all customers purchasing a PTPD-DEV1

### Hardware features
 - 12v input with Reverse Polarity protection and TVS/ESD protection
 - Infineon Profet BTS443P (qty 4) for Power Switches
   - Spade terminals for outputs
   - Each Output has an LED for indicating outputs behavior
   - BTS443P has hardware protection for short circuit and overtemp.
 - Texas Instruments SN65HVD230 CAN Transceiver
   - with external TVS/ESD protection
   - Termination resistor connectable with solder bridge
 - TMP235A Analog Temp Sensor 
 - Resisitor bridge (4:1) for VBATT sensing
 - 4 Switch inputs (TVS\ESD Protected)
 - Onboard Neopixel LED Strip
 - Connector for External Neopixel LEDs
 - JST XH Connectors for Inputs
 - M4 Hole for Ring Terminal for VBATT Power Input
   - Optionally, there is a solder bridge that can join the Battery Ring Connection to the 12v Input for VIN

## Firmware

The current state of the PTPD-DEV firmware is at feature parity with the PTPDSS4 (v1.3 RC1)

### Firmware features
  - Serial Command Line for Programming
    - Open a Serial Monitor and type HELP to see the full list of commands
  -  Battery Voltage Monitoring
    - VBATT voltage levels reported over CAN
    - VBATT voltage level changes Neo Pixel colour
  - Board Temperature monitoring
    - Board Temp  




**Software**: Python-based GUI for real-time monitoring, configuration management, and firmware updates via USB serial interface.

**Key Features**: Live diagnostics, automated protection systems, hardware watchdog, configurable thresholds, and professional operator interface.


##Licence
Copyright (c) 2025 - PT Motorsport AU Pty Ltd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
