"""
Main Window for PDM Manager
Professional GUI interface for PDM configuration and monitoring
"""

import customtkinter as ctk
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import threading
import time
from typing import Optional, Dict, Any
import os

from pdm_communication import PDMCommunication
from firmware_updater import FirmwareUpdater
from gui.config_panel import ConfigurationPanel

class PDMManagerApp:
    """Main PDM Manager Application"""
    
    def __init__(self):
        # Initialize communication and firmware updater
        self.pdm_comm = PDMCommunication()
        self.firmware_updater = FirmwareUpdater()
        
        # Initialize main window
        self.root = ctk.CTk()
        self.root.title("PT Motorsport PDM Manager v1.0")
        self.root.geometry("1000x700")
        self.root.minsize(900, 600)
        
        # Set professional color scheme
        ctk.set_appearance_mode("light")
        
        # Initialize variables
        self.connection_status = tk.StringVar(value="Disconnected")
        self.selected_port = tk.StringVar()
        self.device_info = {
            "firmware_version": "Unknown",
            "uptime": 0,
            "temperature": 0.0,
            "battery_voltage": 0.0
        }
        self.channel_data = [
            {"active": False, "current": 0.0, "mode": "L", "group": 1, "led_state": "OFF"}
            for _ in range(4)
        ]
        
        # Setup GUI components
        self.setup_gui()
        
        # Setup communication callback
        self.pdm_comm.set_status_callback(self.on_status_update)
        
        # Start periodic updates
        self.setup_periodic_updates()
        
    def update_status_callback(self, message: str):
        """Callback for status updates from configuration panel"""
        self.status_bar_label.configure(text=message)
        
    def setup_gui(self):
        """Initialize GUI components"""
        
        # Main header frame
        self.setup_header()
        
        # Main content area with tabs
        self.setup_main_content()
        
        # Status bar
        self.setup_status_bar()
        
    def setup_header(self):
        """Setup header with connection controls"""
        header_frame = ctk.CTkFrame(self.root, height=80)
        header_frame.pack(fill="x", padx=10, pady=(10, 5))
        header_frame.pack_propagate(False)
        
        # Company logo/title
        title_label = ctk.CTkLabel(
            header_frame, 
            text="PT Motorsport PDM Manager", 
            font=ctk.CTkFont(size=24, weight="bold")
        )
        title_label.pack(side="left", padx=20, pady=20)
        
        # Connection controls frame
        conn_frame = ctk.CTkFrame(header_frame)
        conn_frame.pack(side="right", padx=20, pady=15)
        
        # Port selection
        ctk.CTkLabel(conn_frame, text="Port:", font=ctk.CTkFont(weight="bold")).grid(
            row=0, column=0, padx=5, pady=5, sticky="w"
        )
        
        self.port_combo = ctk.CTkComboBox(
            conn_frame, 
            variable=self.selected_port,
            values=self.get_available_ports(),
            width=100
        )
        self.port_combo.grid(row=0, column=1, padx=5, pady=5)
        
        # Refresh ports button
        self.refresh_btn = ctk.CTkButton(
            conn_frame, 
            text="🔄", 
            width=30,
            command=self.refresh_ports
        )
        self.refresh_btn.grid(row=0, column=2, padx=2, pady=5)
        
        # Connect/Disconnect button
        self.connect_btn = ctk.CTkButton(
            conn_frame,
            text="Connect",
            width=100,
            command=self.toggle_connection
        )
        self.connect_btn.grid(row=0, column=3, padx=10, pady=5)
        
        # Connection status
        self.status_label = ctk.CTkLabel(
            conn_frame,
            textvariable=self.connection_status,
            font=ctk.CTkFont(weight="bold")
        )
        self.status_label.grid(row=1, column=0, columnspan=4, pady=(0, 5))
        
    def setup_main_content(self):
        """Setup main tabbed interface"""
        # Create notebook for tabs
        self.notebook = ttk.Notebook(self.root)
        self.notebook.pack(fill="both", expand=True, padx=10, pady=5)
        
        # Monitor Tab
        self.setup_monitor_tab()
        
        # Configuration Tab
        self.setup_config_tab()
        
        # Firmware Tab
        self.setup_firmware_tab()
        
        # Diagnostics Tab
        self.setup_diagnostics_tab()
        
    def setup_monitor_tab(self):
        """Setup real-time monitoring tab"""
        monitor_frame = ctk.CTkFrame(self.notebook)
        self.notebook.add(monitor_frame, text="Monitor")
        
        # Device status section
        status_frame = ctk.CTkFrame(monitor_frame)
        status_frame.pack(fill="x", padx=10, pady=10)
        
        ctk.CTkLabel(
            status_frame, 
            text="Device Status", 
            font=ctk.CTkFont(size=18, weight="bold")
        ).pack(pady=10)
        
        # Status grid
        status_grid = ctk.CTkFrame(status_frame)
        status_grid.pack(pady=10)
        
        # Temperature
        ctk.CTkLabel(status_grid, text="Temperature:", font=ctk.CTkFont(weight="bold")).grid(
            row=0, column=0, padx=10, pady=5, sticky="w"
        )
        self.temp_label = ctk.CTkLabel(status_grid, text="-- °C")
        self.temp_label.grid(row=0, column=1, padx=10, pady=5, sticky="w")
        
        # Battery Voltage
        ctk.CTkLabel(status_grid, text="Battery:", font=ctk.CTkFont(weight="bold")).grid(
            row=0, column=2, padx=10, pady=5, sticky="w"
        )
        self.battery_label = ctk.CTkLabel(status_grid, text="-- V")
        self.battery_label.grid(row=0, column=3, padx=10, pady=5, sticky="w")
        
        # Uptime
        ctk.CTkLabel(status_grid, text="Uptime:", font=ctk.CTkFont(weight="bold")).grid(
            row=1, column=0, padx=10, pady=5, sticky="w"
        )
        self.uptime_label = ctk.CTkLabel(status_grid, text="-- seconds")
        self.uptime_label.grid(row=1, column=1, padx=10, pady=5, sticky="w")
        
        # Firmware Version
        ctk.CTkLabel(status_grid, text="Firmware:", font=ctk.CTkFont(weight="bold")).grid(
            row=1, column=2, padx=10, pady=5, sticky="w"
        )
        self.firmware_label = ctk.CTkLabel(status_grid, text="--")
        self.firmware_label.grid(row=1, column=3, padx=10, pady=5, sticky="w")
        
        # Channel status section
        channels_frame = ctk.CTkFrame(monitor_frame)
        channels_frame.pack(fill="both", expand=True, padx=10, pady=10)
        
        ctk.CTkLabel(
            channels_frame, 
            text="Channel Status", 
            font=ctk.CTkFont(size=18, weight="bold")
        ).pack(pady=10)
        
        # Channel grid
        self.setup_channel_display(channels_frame)
        
    def setup_channel_display(self, parent):
        """Setup channel status display"""
        channels_grid = ctk.CTkFrame(parent)
        channels_grid.pack(expand=True, fill="both", padx=10, pady=10)
        
        # Headers
        headers = ["Ch", "Status", "Current", "Mode", "Group", "LED"]
        for i, header in enumerate(headers):
            ctk.CTkLabel(
                channels_grid, 
                text=header, 
                font=ctk.CTkFont(weight="bold")
            ).grid(row=0, column=i, padx=10, pady=5)
        
        # Channel rows
        self.channel_widgets = []
        for ch in range(4):
            row_widgets = {}
            
            # Channel number
            ctk.CTkLabel(channels_grid, text=f"{ch+1}").grid(
                row=ch+1, column=0, padx=10, pady=5
            )
            
            # Status indicator
            row_widgets['status'] = ctk.CTkLabel(channels_grid, text="OFF")
            row_widgets['status'].grid(row=ch+1, column=1, padx=10, pady=5)
            
            # Current reading
            row_widgets['current'] = ctk.CTkLabel(channels_grid, text="0.00 A")
            row_widgets['current'].grid(row=ch+1, column=2, padx=10, pady=5)
            
            # Mode
            row_widgets['mode'] = ctk.CTkLabel(channels_grid, text="L")
            row_widgets['mode'].grid(row=ch+1, column=3, padx=10, pady=5)
            
            # Group
            row_widgets['group'] = ctk.CTkLabel(channels_grid, text="1")
            row_widgets['group'].grid(row=ch+1, column=4, padx=10, pady=5)
            
            # LED State
            row_widgets['led'] = ctk.CTkLabel(channels_grid, text="OFF")
            row_widgets['led'].grid(row=ch+1, column=5, padx=10, pady=5)
            
            self.channel_widgets.append(row_widgets)
    
    def setup_config_tab(self):
        """Setup configuration tab"""
        config_frame = ctk.CTkFrame(self.notebook)
        self.notebook.add(config_frame, text="Configuration")
        
        # Create configuration panel
        self.config_panel = ConfigurationPanel(
            config_frame,
            self.pdm_comm,
            self.update_status_callback
        )
        
    def setup_firmware_tab(self):
        """Setup firmware update tab"""
        firmware_frame = ctk.CTkFrame(self.notebook)
        self.notebook.add(firmware_frame, text="Firmware")
        
        ctk.CTkLabel(
            firmware_frame, 
            text="Firmware Update", 
            font=ctk.CTkFont(size=18, weight="bold")
        ).pack(pady=20)
        
        # Current firmware info
        info_frame = ctk.CTkFrame(firmware_frame)
        info_frame.pack(fill="x", padx=50, pady=20)
        
        ctk.CTkLabel(info_frame, text="Current Firmware:", font=ctk.CTkFont(weight="bold")).pack(anchor="w", padx=20, pady=5)
        self.current_fw_label = ctk.CTkLabel(info_frame, text="Unknown")
        self.current_fw_label.pack(anchor="w", padx=40, pady=5)
        
        # Firmware file selection
        file_frame = ctk.CTkFrame(firmware_frame)
        file_frame.pack(fill="x", padx=50, pady=20)
        
        ctk.CTkLabel(file_frame, text="Select Firmware File:", font=ctk.CTkFont(weight="bold")).pack(anchor="w", padx=20, pady=5)
        
        file_select_frame = ctk.CTkFrame(file_frame)
        file_select_frame.pack(fill="x", padx=20, pady=10)
        
        self.firmware_file_var = tk.StringVar()
        self.firmware_file_entry = ctk.CTkEntry(
            file_select_frame, 
            textvariable=self.firmware_file_var,
            placeholder_text="No file selected...",
            width=400
        )
        self.firmware_file_entry.pack(side="left", padx=5, pady=5)
        
        ctk.CTkButton(
            file_select_frame,
            text="Browse",
            command=self.browse_firmware_file
        ).pack(side="left", padx=5, pady=5)
        
        # Update button
        self.update_fw_btn = ctk.CTkButton(
            firmware_frame,
            text="Update Firmware",
            font=ctk.CTkFont(size=16, weight="bold"),
            height=40,
            command=self.start_firmware_update
        )
        self.update_fw_btn.pack(pady=30)
        
        # Progress bar
        self.fw_progress = ctk.CTkProgressBar(firmware_frame, width=400)
        self.fw_progress.pack(pady=10)
        self.fw_progress.set(0)
        
        # Status text
        self.fw_status_label = ctk.CTkLabel(firmware_frame, text="Ready")
        self.fw_status_label.pack(pady=10)
        
    def setup_diagnostics_tab(self):
        """Setup diagnostics tab"""
        diag_frame = ctk.CTkFrame(self.notebook)
        self.notebook.add(diag_frame, text="Diagnostics")
        
        ctk.CTkLabel(
            diag_frame, 
            text="Diagnostic Tools", 
            font=ctk.CTkFont(size=18, weight="bold")
        ).pack(pady=20)
        
        # Diagnostics will be implemented in next iteration
        ctk.CTkLabel(
            diag_frame, 
            text="Diagnostic tools coming soon...",
            font=ctk.CTkFont(size=14)
        ).pack(pady=50)
        
    def setup_status_bar(self):
        """Setup status bar"""
        status_frame = ctk.CTkFrame(self.root, height=30)
        status_frame.pack(fill="x", padx=10, pady=(0, 10))
        status_frame.pack_propagate(False)
        
        self.status_bar_label = ctk.CTkLabel(
            status_frame, 
            text="Ready",
            font=ctk.CTkFont(size=12)
        )
        self.status_bar_label.pack(side="left", padx=10, pady=5)
        
        # Version info
        version_label = ctk.CTkLabel(
            status_frame,
            text="v1.0",
            font=ctk.CTkFont(size=12)
        )
        version_label.pack(side="right", padx=10, pady=5)
        
    def get_available_ports(self):
        """Get available COM ports"""
        return self.pdm_comm.get_available_ports()
        
    def refresh_ports(self):
        """Refresh available ports"""
        ports = self.get_available_ports()
        self.port_combo.configure(values=ports)
        if ports and not self.selected_port.get():
            self.selected_port.set(ports[0])
            
    def toggle_connection(self):
        """Connect or disconnect from PDM"""
        if self.pdm_comm.is_connected:
            self.disconnect_pdm()
        else:
            self.connect_pdm()
            
    def connect_pdm(self):
        """Connect to PDM device"""
        port = self.selected_port.get()
        if not port:
            messagebox.showerror("Error", "Please select a COM port")
            return
            
        self.status_bar_label.configure(text="Connecting...")
        self.connect_btn.configure(text="Connecting...", state="disabled")
        
        # Connect in background thread
        def connect_thread():
            success = self.pdm_comm.connect(port)
            
            # Update UI in main thread
            self.root.after(0, self.on_connection_result, success)
            
        threading.Thread(target=connect_thread, daemon=True).start()
        
    def on_connection_result(self, success: bool):
        """Handle connection result"""
        if success:
            self.connection_status.set(f"Connected - {self.pdm_comm.port_name}")
            self.connect_btn.configure(text="Disconnect", state="normal")
            self.status_bar_label.configure(text="Connected successfully")
            
            # Request initial status
            self.request_device_status()
        else:
            self.connection_status.set("Connection Failed")
            self.connect_btn.configure(text="Connect", state="normal")
            self.status_bar_label.configure(text="Connection failed")
            messagebox.showerror("Connection Error", "Failed to connect to PDM device")
            
    def disconnect_pdm(self):
        """Disconnect from PDM device"""
        self.pdm_comm.disconnect()
        self.connection_status.set("Disconnected")
        self.connect_btn.configure(text="Connect")
        self.status_bar_label.configure(text="Disconnected")
        
    def on_status_update(self, update_type: str, data: Any):
        """Handle real-time status updates from PDM"""
        if update_type == "temperature":
            self.device_info["temperature"] = data
            self.root.after(0, self.update_temperature_display)
        elif update_type == "battery_voltage":
            self.device_info["battery_voltage"] = data
            self.root.after(0, self.update_battery_display)
        elif update_type == "channel_status":
            ch = data["channel"]
            if 0 <= ch < 4:
                self.channel_data[ch].update(data)
                self.root.after(0, lambda: self.update_channel_display(ch))
                
    def update_temperature_display(self):
        """Update temperature display"""
        temp = self.device_info["temperature"]
        self.temp_label.configure(text=f"{temp:.1f} °C")
        
    def update_battery_display(self):
        """Update battery voltage display"""
        voltage = self.device_info["battery_voltage"]
        self.battery_label.configure(text=f"{voltage:.1f} V")
        
    def update_channel_display(self, channel: int):
        """Update specific channel display"""
        if 0 <= channel < len(self.channel_widgets):
            data = self.channel_data[channel]
            widgets = self.channel_widgets[channel]
            
            widgets['status'].configure(text="ON" if data["active"] else "OFF")
            widgets['current'].configure(text=f"{data['current']:.2f} A")
            widgets['mode'].configure(text=data["mode"])
            widgets['group'].configure(text=str(data["group"]))
            widgets['led'].configure(text=data["led_state"])
            
    def request_device_status(self):
        """Request complete device status"""
        if self.pdm_comm.is_connected:
            def status_thread():
                status = self.pdm_comm.get_device_status()
                if status:
                    self.root.after(0, lambda: self.update_device_status(status))
                    
            threading.Thread(target=status_thread, daemon=True).start()
            
    def update_device_status(self, status: Dict):
        """Update all device status displays"""
        self.device_info.update(status)
        
        # Update displays
        self.update_temperature_display()
        self.update_battery_display()
        self.uptime_label.configure(text=f"{status['uptime']} seconds")
        
        # Update channel displays
        for i, ch_data in enumerate(status["channels"]):
            self.channel_data[i].update(ch_data)
            self.update_channel_display(i)
            
    def browse_firmware_file(self):
        """Browse for firmware file"""
        filename = filedialog.askopenfilename(
            title="Select Firmware File",
            filetypes=[("Intel HEX files", "*.hex"), ("All files", "*.*")]
        )
        
        if filename:
            if self.firmware_updater.verify_hex_file(filename):
                self.firmware_file_var.set(filename)
                self.fw_status_label.configure(text="Firmware file selected")
            else:
                messagebox.showerror("Invalid File", "Selected file does not appear to be a valid firmware file")
                
    def start_firmware_update(self):
        """Start firmware update process"""
        if not self.pdm_comm.is_connected:
            messagebox.showerror("Error", "Please connect to PDM device first")
            return
            
        firmware_file = self.firmware_file_var.get()
        if not firmware_file:
            messagebox.showerror("Error", "Please select a firmware file")
            return
            
        if not self.firmware_updater.is_available():
            messagebox.showerror("Error", "Arduino CLI not found. Please install Arduino CLI or place it in the tools folder.")
            return
            
        # Confirm update
        result = messagebox.askyesno(
            "Confirm Firmware Update",
            "This will update the PDM firmware. The device will be temporarily unavailable during the update.\n\nContinue?"
        )
        
        if not result:
            return
            
        # Disconnect from device for update
        port = self.pdm_comm.port_name
        self.disconnect_pdm()
        
        # Start update
        self.update_fw_btn.configure(state="disabled")
        self.fw_progress.set(0)
        
        def progress_callback(msg_type: str, message: str):
            self.root.after(0, lambda: self.on_firmware_progress(msg_type, message))
            
        self.firmware_updater.update_firmware_async(
            firmware_file, port, progress_callback
        )
        
    def on_firmware_progress(self, msg_type: str, message: str):
        """Handle firmware update progress"""
        if msg_type == "info":
            self.fw_status_label.configure(text=message)
        elif msg_type == "error":
            self.fw_status_label.configure(text=f"Error: {message}")
            self.fw_progress.set(0)
            self.update_fw_btn.configure(state="normal")
            messagebox.showerror("Update Failed", message)
        elif msg_type == "success":
            self.fw_status_label.configure(text=message)
            self.fw_progress.set(1.0)
            self.update_fw_btn.configure(state="normal")
            messagebox.showinfo("Update Complete", message)
        elif msg_type == "progress":
            if message is not None:
                self.fw_progress.set(message / 100.0)
            # For indeterminate progress, just pulse the bar
            
    def setup_periodic_updates(self):
        """Setup periodic status updates"""
        def update_loop():
            if self.pdm_comm.is_connected:
                self.request_device_status()
            
            # Schedule next update
            self.root.after(2000, update_loop)  # Update every 2 seconds
            
        # Start the update loop
        self.root.after(1000, update_loop)
        
    def run(self):
        """Run the application"""
        # Refresh ports on startup
        self.refresh_ports()
        
        # Start main loop
        self.root.mainloop()
        
        # Cleanup on exit
        if self.pdm_comm.is_connected:
            self.pdm_comm.disconnect()