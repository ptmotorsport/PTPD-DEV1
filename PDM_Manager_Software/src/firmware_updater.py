"""
Firmware Update Module
Handles PDM firmware updates using Arduino CLI
"""

import os
import subprocess
import threading
import time
from typing import Optional, Callable
import tempfile
import shutil

class FirmwareUpdater:
    """Manages PDM firmware updates"""
    
    def __init__(self):
        self.arduino_cli_path = self._find_arduino_cli()
        self.update_callback: Optional[Callable] = None
        self.is_updating = False
        
    def _find_arduino_cli(self) -> Optional[str]:
        """Find Arduino CLI executable"""
        # Check common installation paths
        common_paths = [
            "arduino-cli.exe",
            "C:\\Program Files\\Arduino IDE\\arduino-cli.exe",
            "C:\\Users\\*\\AppData\\Local\\Arduino15\\arduino-cli.exe",
            os.path.join(os.path.dirname(__file__), "..", "tools", "arduino-cli.exe")
        ]
        
        for path in common_paths:
            if os.path.exists(path):
                return path
                
        # Try to find in PATH
        try:
            result = subprocess.run(["where", "arduino-cli"], 
                                  capture_output=True, text=True, shell=True)
            if result.returncode == 0:
                return result.stdout.strip().split('\n')[0]
        except:
            pass
            
        return None
    
    def is_available(self) -> bool:
        """Check if firmware update capability is available"""
        return self.arduino_cli_path is not None
    
    def get_board_fqbn(self) -> str:
        """Get the fully qualified board name for Arduino UNO R4 Minima"""
        return "arduino:renesas_uno:unor4minima"
    
    def update_firmware(self, hex_file_path: str, port: str, 
                       progress_callback: Optional[Callable] = None) -> bool:
        """
        Update PDM firmware
        
        Args:
            hex_file_path: Path to firmware .hex file
            port: COM port (e.g., "COM3")
            progress_callback: Function to call with progress updates
            
        Returns:
            True if update successful, False otherwise
        """
        if not self.arduino_cli_path:
            if progress_callback:
                progress_callback("error", "Arduino CLI not found")
            return False
            
        if not os.path.exists(hex_file_path):
            if progress_callback:
                progress_callback("error", f"Firmware file not found: {hex_file_path}")
            return False
            
        self.is_updating = True
        
        try:
            # Prepare upload command
            cmd = [
                self.arduino_cli_path,
                "upload",
                "-p", port,
                "--fqbn", self.get_board_fqbn(),
                "--input-file", hex_file_path,
                "--verify"
            ]
            
            if progress_callback:
                progress_callback("info", "Starting firmware update...")
                progress_callback("progress", 10)
            
            # Execute upload
            process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                universal_newlines=True
            )
            
            # Monitor progress
            while process.poll() is None:
                if progress_callback:
                    progress_callback("progress", None)  # Indeterminate progress
                time.sleep(0.5)
            
            stdout, stderr = process.communicate()
            
            if process.returncode == 0:
                if progress_callback:
                    progress_callback("progress", 100)
                    progress_callback("success", "Firmware update completed successfully!")
                return True
            else:
                error_msg = stderr or stdout or "Unknown error"
                if progress_callback:
                    progress_callback("error", f"Upload failed: {error_msg}")
                return False
                
        except Exception as e:
            if progress_callback:
                progress_callback("error", f"Update failed: {str(e)}")
            return False
        finally:
            self.is_updating = False
    
    def update_firmware_async(self, hex_file_path: str, port: str,
                             progress_callback: Optional[Callable] = None) -> threading.Thread:
        """
        Update firmware asynchronously
        
        Returns:
            Thread object for the update operation
        """
        thread = threading.Thread(
            target=self.update_firmware,
            args=(hex_file_path, port, progress_callback),
            daemon=True
        )
        thread.start()
        return thread
    
    def verify_hex_file(self, hex_file_path: str) -> bool:
        """
        Verify that the hex file is valid
        
        Args:
            hex_file_path: Path to hex file
            
        Returns:
            True if file appears to be a valid Intel HEX file
        """
        try:
            with open(hex_file_path, 'r') as f:
                first_line = f.readline().strip()
                
            # Intel HEX files start with ':'
            if not first_line.startswith(':'):
                return False
                
            # Check for reasonable file size (should be 10KB - 2MB for Arduino)
            file_size = os.path.getsize(hex_file_path)
            if file_size < 1000 or file_size > 2000000:
                return False
                
            return True
            
        except Exception:
            return False
    
    def get_arduino_cli_version(self) -> Optional[str]:
        """Get Arduino CLI version info"""
        if not self.arduino_cli_path:
            return None
            
        try:
            result = subprocess.run(
                [self.arduino_cli_path, "version"],
                capture_output=True,
                text=True,
                timeout=10
            )
            
            if result.returncode == 0:
                return result.stdout.strip()
            else:
                return None
                
        except Exception:
            return None