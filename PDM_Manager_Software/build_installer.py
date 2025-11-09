"""
Build script for creating standalone PDM Manager executable
"""

import PyInstaller.__main__
import os
import sys
import shutil

def build_executable():
    """Build standalone executable using PyInstaller"""
    
    # Define paths
    script_dir = os.path.dirname(os.path.abspath(__file__))
    src_dir = os.path.join(script_dir, "src")
    main_script = os.path.join(script_dir, "main.py")
    dist_dir = os.path.join(script_dir, "dist")
    build_dir = os.path.join(script_dir, "build")
    
    # Clean previous builds
    if os.path.exists(dist_dir):
        shutil.rmtree(dist_dir)
    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)
    
    # PyInstaller arguments
    args = [
        main_script,
        "--onefile",  # Create single executable
        "--windowed",  # No console window
        "--name=PDM_Manager",
        "--icon=resources/icon.ico",  # Add icon if available
        f"--distpath={dist_dir}",
        f"--workpath={build_dir}",
        "--add-data=resources;resources",  # Include resources
        "--hidden-import=customtkinter",
        "--hidden-import=serial",
        "--hidden-import=PIL",
        "--collect-all=customtkinter",
        "--noconsole"
    ]
    
    print("Building PDM Manager executable...")
    print(f"Main script: {main_script}")
    print(f"Output directory: {dist_dir}")
    
    try:
        # Run PyInstaller
        PyInstaller.__main__.run(args)
        
        print("\n✓ Build completed successfully!")
        print(f"Executable created: {os.path.join(dist_dir, 'PDM_Manager.exe')}")
        
        # Create distribution package
        create_distribution_package()
        
    except Exception as e:
        print(f"✗ Build failed: {e}")
        return False
    
    return True

def create_distribution_package():
    """Create complete distribution package"""
    
    script_dir = os.path.dirname(os.path.abspath(__file__))
    dist_dir = os.path.join(script_dir, "dist")
    package_dir = os.path.join(dist_dir, "PDM_Manager_v1.0")
    
    # Create package directory
    os.makedirs(package_dir, exist_ok=True)
    
    # Copy executable
    exe_src = os.path.join(dist_dir, "PDM_Manager.exe")
    exe_dst = os.path.join(package_dir, "PDM_Manager.exe")
    if os.path.exists(exe_src):
        shutil.copy2(exe_src, exe_dst)
    
    # Create tools directory and copy Arduino CLI if available
    tools_dir = os.path.join(package_dir, "tools")
    os.makedirs(tools_dir, exist_ok=True)
    
    # Copy documentation
    docs_to_copy = ["README.md"]
    for doc in docs_to_copy:
        src_path = os.path.join(script_dir, doc)
        if os.path.exists(src_path):
            shutil.copy2(src_path, package_dir)
    
    # Create Quick Start Guide
    create_quick_start_guide(package_dir)
    
    print(f"✓ Distribution package created: {package_dir}")

def create_quick_start_guide(package_dir):
    """Create a quick start guide for customers"""
    
    guide_content = """# PDM Manager Quick Start Guide

## Installation
1. Extract all files to a folder on your computer
2. Ensure your PDM device is connected via USB
3. Double-click PDM_Manager.exe to start

## First Time Setup
1. Select the correct COM port from the dropdown
2. Click "Connect" to establish communication
3. The Monitor tab will show real-time device status

## Updating Firmware
1. Go to the "Firmware" tab
2. Click "Browse" and select your .hex firmware file
3. Click "Update Firmware" and wait for completion
4. The device will restart automatically

## Troubleshooting

### Connection Issues
- Check USB cable connection
- Try different COM ports
- Restart the application
- Check Windows Device Manager for port conflicts

### Firmware Update Issues
- Ensure device is connected before starting update
- Use only .hex files provided by PT Motorsport
- Do not disconnect during update process

## Support
For technical support, contact PT Motorsport with:
- Software version (shown in status bar)
- Device firmware version (shown in Monitor tab)
- Description of the issue

## Version: 1.0
Built: """ + str(__import__('datetime').datetime.now().strftime('%Y-%m-%d %H:%M'))
    
    guide_path = os.path.join(package_dir, "Quick_Start_Guide.txt")
    with open(guide_path, 'w') as f:
        f.write(guide_content)

if __name__ == "__main__":
    build_executable()