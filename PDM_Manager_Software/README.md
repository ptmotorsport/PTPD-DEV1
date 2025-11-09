# PDM Manager Software
PT Motorsport Professional PDM Management Software

## Overview
Complete customer software package for managing PT Motorsport PDM devices including:
- Real-time device monitoring
- Configuration management
- Firmware updates
- Diagnostic tools

## Features
- Live status monitoring (channels, temperature, battery voltage)
- Interactive parameter configuration
- One-click firmware updates
- Professional GUI interface
- Configuration import/export
- Diagnostic logging

## Installation
1. Install Python 3.9+ 
2. Install dependencies: `pip install -r requirements.txt`
3. Run: `python src/main.py`

## Building Standalone Executable
```bash
python build_installer.py
```

## System Requirements
- Windows 10/11 (primary target)
- USB serial drivers for Arduino UNO R4 Minima
- 50MB disk space

## Version History
- v1.0 - Initial release with core functionality