"""
PDM Serial Communication Module
Handles all serial communication with PDM devices
"""

import serial
import serial.tools.list_ports
import threading
import time
import re
from typing import Optional, Dict, List, Callable
import queue

class PDMCommunication:
    """Manages serial communication with PDM devices"""
    
    def __init__(self):
        self.serial_port: Optional[serial.Serial] = None
        self.is_connected = False
        self.port_name = ""
        self.read_thread: Optional[threading.Thread] = None
        self.running = False
        self.response_queue = queue.Queue()
        self.status_callback: Optional[Callable] = None
        
        # Communication settings
        self.baudrate = 115200
        self.timeout = 2.0
        self.command_timeout = 3.0
        
    def get_available_ports(self) -> List[str]:
        """Get list of available COM ports"""
        ports = []
        for port in serial.tools.list_ports.comports():
            ports.append(port.device)
        return ports
    
    def connect(self, port: str) -> bool:
        """Connect to PDM on specified port"""
        try:
            if self.is_connected:
                self.disconnect()
                
            self.serial_port = serial.Serial(
                port=port,
                baudrate=self.baudrate,
                timeout=self.timeout,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                bytesize=serial.EIGHTBITS
            )
            
            # Test connection with a simple command
            time.sleep(2)  # Wait for Arduino to reset
            self.serial_port.flushInput()
            self.serial_port.flushOutput()
            
            # Try to get a response
            self.serial_port.write(b"\r\n")
            time.sleep(0.5)
            self.serial_port.write(b"STATUS\r\n")
            
            # Start read thread
            self.running = True
            self.read_thread = threading.Thread(target=self._read_loop, daemon=True)
            self.read_thread.start()
            
            self.is_connected = True
            self.port_name = port
            return True
            
        except Exception as e:
            if self.serial_port:
                self.serial_port.close()
            self.is_connected = False
            print(f"Connection failed: {e}")
            return False
    
    def disconnect(self):
        """Disconnect from PDM"""
        self.running = False
        self.is_connected = False
        
        if self.read_thread and self.read_thread.is_alive():
            self.read_thread.join(timeout=1.0)
            
        if self.serial_port:
            self.serial_port.close()
            self.serial_port = None
            
        self.port_name = ""
    
    def _read_loop(self):
        """Background thread for reading serial data"""
        while self.running and self.serial_port and self.serial_port.is_open:
            try:
                line = self.serial_port.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    self.response_queue.put(line)
                    
                    # Parse status updates if callback is set
                    if self.status_callback:
                        self._parse_status_line(line)
                        
            except Exception as e:
                if self.running:  # Only log if we're supposed to be running
                    print(f"Read error: {e}")
                break
    
    def _parse_status_line(self, line: str):
        """Parse incoming status data and call callback"""
        # Parse various status patterns
        if "Board Temperature:" in line:
            match = re.search(r"Board Temperature:\s*([-\d.]+)", line)
            if match and self.status_callback:
                self.status_callback("temperature", float(match.group(1)))
                
        elif "Battery Voltage:" in line:
            match = re.search(r"Battery Voltage:\s*([\d.]+)", line)
            if match and self.status_callback:
                self.status_callback("battery_voltage", float(match.group(1)))
                
        elif "|" in line and ("ON" in line or "OFF" in line):
            # Channel status line
            self._parse_channel_status(line)
    
    def _parse_channel_status(self, line: str):
        """Parse channel status from STATUS command output"""
        try:
            # Example: "1  |   ON   | 2.50 A |  L  |   1   |   GREEN  | OK"
            parts = [p.strip() for p in line.split("|")]
            if len(parts) >= 6:
                ch = int(parts[0]) - 1  # Convert to 0-based index
                state = parts[1] == "ON"
                current = float(parts[2].replace("A", "").strip())
                mode = parts[3].strip()
                group = int(parts[4])
                led_state = parts[5].strip()
                
                if self.status_callback:
                    self.status_callback("channel_status", {
                        "channel": ch,
                        "active": state,
                        "current": current,
                        "mode": mode,
                        "group": group,
                        "led_state": led_state
                    })
        except (ValueError, IndexError):
            pass  # Ignore malformed lines
    
    def send_command(self, command: str) -> Optional[str]:
        """Send command and wait for response"""
        if not self.is_connected or not self.serial_port:
            return None
            
        try:
            # Clear any pending responses
            while not self.response_queue.empty():
                self.response_queue.get_nowait()
                
            # Send command
            self.serial_port.write(f"{command}\r\n".encode())
            self.serial_port.flush()
            
            # Wait for response
            start_time = time.time()
            while time.time() - start_time < self.command_timeout:
                try:
                    response = self.response_queue.get(timeout=0.1)
                    if response and not response.startswith("Received:"):
                        return response
                except queue.Empty:
                    continue
                    
            return None
            
        except Exception as e:
            print(f"Command failed: {e}")
            return None
    
    def send_config_command(self, command: str) -> bool:
        """Send configuration command and check for OK response"""
        response = self.send_command(command)
        return response and response.startswith("OK:")
    
    def get_current_configuration(self) -> Optional[Dict]:
        """Get current device configuration"""
        if not self.is_connected:
            return None
            
        config_data = {}
        
        # Get basic device info first
        status_response = self.send_command("STATUS")
        if status_response:
            config_data["status"] = status_response
            
        # You can add more specific configuration queries here
        # For example, if your firmware supports a CONFIG command:
        # config_response = self.send_command("CONFIG")
        
        return config_data
    
    def get_device_status(self) -> Optional[Dict]:
        """Get complete device status"""
        if not self.is_connected:
            return None
            
        # Send STATUS command and parse multi-line response
        self.send_command("STATUS")
        
        # Collect status lines
        status_data = {
            "channels": [{"active": False, "current": 0.0, "mode": "L", "group": 1} for _ in range(4)],
            "temperature": 0.0,
            "battery_voltage": 0.0,
            "uptime": 0
        }
        
        # Read multiple lines of status
        start_time = time.time()
        while time.time() - start_time < 2.0:  # 2 second timeout
            try:
                line = self.response_queue.get(timeout=0.1)
                if "====" in line and "STATUS" in line:
                    break  # End of status block
                    
                # Parse specific status lines
                if "Board Temperature:" in line:
                    match = re.search(r"([-\d.]+)\s*°?C", line)
                    if match:
                        status_data["temperature"] = float(match.group(1))
                        
                elif "Battery Voltage:" in line:
                    match = re.search(r"([\d.]+)\s*V", line)
                    if match:
                        status_data["battery_voltage"] = float(match.group(1))
                        
                elif "System Uptime:" in line:
                    match = re.search(r"(\d+)\s*seconds", line)
                    if match:
                        status_data["uptime"] = int(match.group(1))
                        
            except queue.Empty:
                continue
                
        return status_data
    
    def set_status_callback(self, callback: Callable):
        """Set callback for real-time status updates"""
        self.status_callback = callback