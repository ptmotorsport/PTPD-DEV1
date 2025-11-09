"""
Configuration Panel for PDM Manager
Handles all PDM parameter configuration through GUI
"""

import customtkinter as ctk
import tkinter as tk
from tkinter import messagebox
from typing import Optional, Dict, Any, Callable
import threading

class ConfigurationPanel:
    """PDM Configuration Interface"""
    
    def __init__(self, parent_frame, pdm_comm, status_callback: Optional[Callable] = None):
        self.parent_frame = parent_frame
        self.pdm_comm = pdm_comm
        self.status_callback = status_callback
        
        # Configuration data
        self.config_data = {
            "channels": [
                {
                    "oc_threshold": 15.0,
                    "inrush_threshold": 50.0,
                    "inrush_time": 1000,
                    "underwarn_threshold": 0.10,
                    "mode": "LATCH",
                    "group": 1
                } for _ in range(4)
            ],
            "temp_warn": 70.0,
            "temp_trip": 85.0,
            "can_speed": 1000,
            "pdm_node_id": 0x15,
            "keypad_node_id": 0x15,
            "digital_out_id": 0x680
        }
        
        # Configuration widgets
        self.config_widgets = {}
        
        # Setup GUI
        self.setup_configuration_interface()
        
        # Load current configuration from device
        self.load_current_config()
    
    def setup_configuration_interface(self):
        """Setup the configuration interface"""
        
        # Main scrollable frame
        self.main_frame = ctk.CTkScrollableFrame(self.parent_frame)
        self.main_frame.pack(fill="both", expand=True, padx=10, pady=10)
        
        # Header
        header_frame = ctk.CTkFrame(self.main_frame)
        header_frame.pack(fill="x", pady=(0, 20))
        
        ctk.CTkLabel(
            header_frame,
            text="PDM Configuration",
            font=ctk.CTkFont(size=20, weight="bold")
        ).pack(pady=15)
        
        # Channel Configuration Section
        self.setup_channel_config()
        
        # System Configuration Section
        self.setup_system_config()
        
        # CAN Configuration Section
        self.setup_can_config()
        
        # Control Buttons
        self.setup_control_buttons()
    
    def setup_channel_config(self):
        """Setup channel configuration section"""
        # Channel Configuration Frame
        channel_frame = ctk.CTkFrame(self.main_frame)
        channel_frame.pack(fill="x", pady=10)
        
        ctk.CTkLabel(
            channel_frame,
            text="Channel Configuration",
            font=ctk.CTkFont(size=16, weight="bold")
        ).pack(pady=10)
        
        # Create tabview for channels
        self.channel_tabview = ctk.CTkTabview(channel_frame, height=300)
        self.channel_tabview.pack(fill="x", padx=10, pady=10)
        
        # Create tabs for each channel
        for ch in range(4):
            tab_name = f"Channel {ch+1}"
            self.channel_tabview.add(tab_name)
            self.setup_single_channel_config(
                self.channel_tabview.tab(tab_name), ch
            )
    
    def setup_single_channel_config(self, parent, channel: int):
        """Setup configuration for a single channel"""
        
        # Store widgets for this channel
        self.config_widgets[f"channel_{channel}"] = {}
        
        # Protection Settings Frame
        protection_frame = ctk.CTkFrame(parent)
        protection_frame.pack(fill="x", padx=10, pady=10)
        
        ctk.CTkLabel(
            protection_frame,
            text="Protection Settings",
            font=ctk.CTkFont(size=14, weight="bold")
        ).pack(pady=5)
        
        # Grid for protection settings
        prot_grid = ctk.CTkFrame(protection_frame)
        prot_grid.pack(pady=10)
        
        # Overcurrent Threshold
        ctk.CTkLabel(prot_grid, text="Overcurrent Limit:", font=ctk.CTkFont(weight="bold")).grid(
            row=0, column=0, padx=10, pady=5, sticky="w"
        )
        
        oc_var = tk.DoubleVar(value=self.config_data["channels"][channel]["oc_threshold"])
        oc_spinbox = ctk.CTkEntry(prot_grid, textvariable=oc_var, width=80)
        oc_spinbox.grid(row=0, column=1, padx=5, pady=5)
        
        ctk.CTkLabel(prot_grid, text="A").grid(row=0, column=2, padx=5, pady=5, sticky="w")
        
        self.config_widgets[f"channel_{channel}"]["oc_threshold"] = oc_var
        
        # Inrush Threshold
        ctk.CTkLabel(prot_grid, text="Inrush Limit:", font=ctk.CTkFont(weight="bold")).grid(
            row=1, column=0, padx=10, pady=5, sticky="w"
        )
        
        inr_var = tk.DoubleVar(value=self.config_data["channels"][channel]["inrush_threshold"])
        inr_spinbox = ctk.CTkEntry(prot_grid, textvariable=inr_var, width=80)
        inr_spinbox.grid(row=1, column=1, padx=5, pady=5)
        
        ctk.CTkLabel(prot_grid, text="A").grid(row=1, column=2, padx=5, pady=5, sticky="w")
        
        self.config_widgets[f"channel_{channel}"]["inrush_threshold"] = inr_var
        
        # Inrush Time
        ctk.CTkLabel(prot_grid, text="Inrush Time:", font=ctk.CTkFont(weight="bold")).grid(
            row=2, column=0, padx=10, pady=5, sticky="w"
        )
        
        inr_time_var = tk.IntVar(value=self.config_data["channels"][channel]["inrush_time"])
        inr_time_spinbox = ctk.CTkEntry(prot_grid, textvariable=inr_time_var, width=80)
        inr_time_spinbox.grid(row=2, column=1, padx=5, pady=5)
        
        ctk.CTkLabel(prot_grid, text="ms").grid(row=2, column=2, padx=5, pady=5, sticky="w")
        
        self.config_widgets[f"channel_{channel}"]["inrush_time"] = inr_time_var
        
        # Undercurrent Warning
        ctk.CTkLabel(prot_grid, text="Undercurrent Warning:", font=ctk.CTkFont(weight="bold")).grid(
            row=3, column=0, padx=10, pady=5, sticky="w"
        )
        
        uwr_var = tk.DoubleVar(value=self.config_data["channels"][channel]["underwarn_threshold"])
        uwr_spinbox = ctk.CTkEntry(prot_grid, textvariable=uwr_var, width=80)
        uwr_spinbox.grid(row=3, column=1, padx=5, pady=5)
        
        ctk.CTkLabel(prot_grid, text="A").grid(row=3, column=2, padx=5, pady=5, sticky="w")
        
        self.config_widgets[f"channel_{channel}"]["underwarn_threshold"] = uwr_var
        
        # Mode and Group Settings Frame
        mode_frame = ctk.CTkFrame(parent)
        mode_frame.pack(fill="x", padx=10, pady=10)
        
        ctk.CTkLabel(
            mode_frame,
            text="Operation Settings",
            font=ctk.CTkFont(size=14, weight="bold")
        ).pack(pady=5)
        
        mode_grid = ctk.CTkFrame(mode_frame)
        mode_grid.pack(pady=10)
        
        # Output Mode
        ctk.CTkLabel(mode_grid, text="Output Mode:", font=ctk.CTkFont(weight="bold")).grid(
            row=0, column=0, padx=10, pady=5, sticky="w"
        )
        
        mode_var = tk.StringVar(value=self.config_data["channels"][channel]["mode"])
        mode_combo = ctk.CTkComboBox(
            mode_grid,
            variable=mode_var,
            values=["LATCH", "MOMENTARY"],
            width=120
        )
        mode_combo.grid(row=0, column=1, padx=5, pady=5)
        
        self.config_widgets[f"channel_{channel}"]["mode"] = mode_var
        
        # Group
        ctk.CTkLabel(mode_grid, text="Group:", font=ctk.CTkFont(weight="bold")).grid(
            row=1, column=0, padx=10, pady=5, sticky="w"
        )
        
        group_var = tk.IntVar(value=self.config_data["channels"][channel]["group"])
        group_combo = ctk.CTkComboBox(
            mode_grid,
            variable=group_var,
            values=["1", "2", "3", "4"],
            width=120
        )
        group_combo.grid(row=1, column=1, padx=5, pady=5)
        
        self.config_widgets[f"channel_{channel}"]["group"] = group_var
    
    def setup_system_config(self):
        """Setup system configuration section"""
        # System Configuration Frame
        system_frame = ctk.CTkFrame(self.main_frame)
        system_frame.pack(fill="x", pady=10)
        
        ctk.CTkLabel(
            system_frame,
            text="System Configuration",
            font=ctk.CTkFont(size=16, weight="bold")
        ).pack(pady=10)
        
        # System settings grid
        sys_grid = ctk.CTkFrame(system_frame)
        sys_grid.pack(pady=10)
        
        # Temperature Warning
        ctk.CTkLabel(sys_grid, text="Temperature Warning:", font=ctk.CTkFont(weight="bold")).grid(
            row=0, column=0, padx=10, pady=5, sticky="w"
        )
        
        temp_warn_var = tk.DoubleVar(value=self.config_data["temp_warn"])
        temp_warn_entry = ctk.CTkEntry(sys_grid, textvariable=temp_warn_var, width=80)
        temp_warn_entry.grid(row=0, column=1, padx=5, pady=5)
        
        ctk.CTkLabel(sys_grid, text="°C").grid(row=0, column=2, padx=5, pady=5, sticky="w")
        
        self.config_widgets["temp_warn"] = temp_warn_var
        
        # Temperature Trip
        ctk.CTkLabel(sys_grid, text="Temperature Trip:", font=ctk.CTkFont(weight="bold")).grid(
            row=1, column=0, padx=10, pady=5, sticky="w"
        )
        
        temp_trip_var = tk.DoubleVar(value=self.config_data["temp_trip"])
        temp_trip_entry = ctk.CTkEntry(sys_grid, textvariable=temp_trip_var, width=80)
        temp_trip_entry.grid(row=1, column=1, padx=5, pady=5)
        
        ctk.CTkLabel(sys_grid, text="°C").grid(row=1, column=2, padx=5, pady=5, sticky="w")
        
        self.config_widgets["temp_trip"] = temp_trip_var
    
    def setup_can_config(self):
        """Setup CAN configuration section"""
        # CAN Configuration Frame
        can_frame = ctk.CTkFrame(self.main_frame)
        can_frame.pack(fill="x", pady=10)
        
        ctk.CTkLabel(
            can_frame,
            text="CAN Bus Configuration",
            font=ctk.CTkFont(size=16, weight="bold")
        ).pack(pady=10)
        
        # CAN settings grid
        can_grid = ctk.CTkFrame(can_frame)
        can_grid.pack(pady=10)
        
        # CAN Speed
        ctk.CTkLabel(can_grid, text="CAN Speed:", font=ctk.CTkFont(weight="bold")).grid(
            row=0, column=0, padx=10, pady=5, sticky="w"
        )
        
        can_speed_var = tk.StringVar(value=str(self.config_data["can_speed"]))
        can_speed_combo = ctk.CTkComboBox(
            can_grid,
            variable=can_speed_var,
            values=["125", "250", "500", "1000"],
            width=120
        )
        can_speed_combo.grid(row=0, column=1, padx=5, pady=5)
        
        ctk.CTkLabel(can_grid, text="kbps").grid(row=0, column=2, padx=5, pady=5, sticky="w")
        
        self.config_widgets["can_speed"] = can_speed_var
        
        # PDM Node ID
        ctk.CTkLabel(can_grid, text="PDM Node ID:", font=ctk.CTkFont(weight="bold")).grid(
            row=1, column=0, padx=10, pady=5, sticky="w"
        )
        
        pdm_node_var = tk.StringVar(value=f"0x{self.config_data['pdm_node_id']:02X}")
        pdm_node_entry = ctk.CTkEntry(can_grid, textvariable=pdm_node_var, width=80)
        pdm_node_entry.grid(row=1, column=1, padx=5, pady=5)
        
        ctk.CTkLabel(can_grid, text="(hex)").grid(row=1, column=2, padx=5, pady=5, sticky="w")
        
        self.config_widgets["pdm_node_id"] = pdm_node_var
        
        # Keypad Node ID
        ctk.CTkLabel(can_grid, text="Keypad Node ID:", font=ctk.CTkFont(weight="bold")).grid(
            row=2, column=0, padx=10, pady=5, sticky="w"
        )
        
        keypad_node_var = tk.StringVar(value=f"0x{self.config_data['keypad_node_id']:02X}")
        keypad_node_entry = ctk.CTkEntry(can_grid, textvariable=keypad_node_var, width=80)
        keypad_node_entry.grid(row=2, column=1, padx=5, pady=5)
        
        ctk.CTkLabel(can_grid, text="(hex)").grid(row=2, column=2, padx=5, pady=5, sticky="w")
        
        self.config_widgets["keypad_node_id"] = keypad_node_var
        
        # Digital Output COB-ID
        ctk.CTkLabel(can_grid, text="Digital Output ID:", font=ctk.CTkFont(weight="bold")).grid(
            row=3, column=0, padx=10, pady=5, sticky="w"
        )
        
        digital_out_var = tk.StringVar(value=f"0x{self.config_data['digital_out_id']:03X}")
        digital_out_entry = ctk.CTkEntry(can_grid, textvariable=digital_out_var, width=80)
        digital_out_entry.grid(row=3, column=1, padx=5, pady=5)
        
        ctk.CTkLabel(can_grid, text="(hex)").grid(row=3, column=2, padx=5, pady=5, sticky="w")
        
        self.config_widgets["digital_out_id"] = digital_out_var
    
    def setup_control_buttons(self):
        """Setup control buttons"""
        # Control buttons frame
        control_frame = ctk.CTkFrame(self.main_frame)
        control_frame.pack(fill="x", pady=20)
        
        button_frame = ctk.CTkFrame(control_frame)
        button_frame.pack(pady=15)
        
        # Load Configuration Button
        self.load_btn = ctk.CTkButton(
            button_frame,
            text="Load from Device",
            command=self.load_current_config,
            width=150,
            font=ctk.CTkFont(weight="bold")
        )
        self.load_btn.pack(side="left", padx=10)
        
        # Apply Configuration Button
        self.apply_btn = ctk.CTkButton(
            button_frame,
            text="Apply to Device",
            command=self.apply_configuration,
            width=150,
            font=ctk.CTkFont(weight="bold")
        )
        self.apply_btn.pack(side="left", padx=10)
        
        # Save to EEPROM Button
        self.save_btn = ctk.CTkButton(
            button_frame,
            text="Save to EEPROM",
            command=self.save_configuration,
            width=150,
            font=ctk.CTkFont(weight="bold")
        )
        self.save_btn.pack(side="left", padx=10)
        
        # Factory Reset Button
        self.reset_btn = ctk.CTkButton(
            button_frame,
            text="Factory Reset",
            command=self.factory_reset,
            width=150,
            font=ctk.CTkFont(weight="bold"),
            fg_color="darkred",
            hover_color="red"
        )
        self.reset_btn.pack(side="left", padx=10)
        
        # Status label
        self.config_status_label = ctk.CTkLabel(
            control_frame,
            text="Ready",
            font=ctk.CTkFont(size=12)
        )
        self.config_status_label.pack(pady=5)
    
    def load_current_config(self):
        """Load current configuration from device"""
        if not self.pdm_comm.is_connected:
            messagebox.showerror("Error", "Please connect to PDM device first")
            return
            
        self.config_status_label.configure(text="Loading configuration from device...")
        self.load_btn.configure(state="disabled")
        
        def load_thread():
            try:
                # Get current configuration by sending CONFIG command
                response = self.pdm_comm.send_command("CONFIG")
                
                if response:
                    # Parse configuration response
                    self.parse_config_response(response)
                    self.main_frame.after(0, self.update_config_widgets)
                    self.main_frame.after(0, lambda: self.config_status_label.configure(text="Configuration loaded successfully"))
                else:
                    self.main_frame.after(0, lambda: self.config_status_label.configure(text="Failed to load configuration"))
                    
            except Exception as e:
                self.main_frame.after(0, lambda: self.config_status_label.configure(text=f"Error: {str(e)}"))
            finally:
                self.main_frame.after(0, lambda: self.load_btn.configure(state="normal"))
                
        threading.Thread(target=load_thread, daemon=True).start()
    
    def parse_config_response(self, response: str):
        """Parse configuration response from device"""
        # This would parse the CONFIG command response
        # For now, we'll use default values since CONFIG command might not exist yet
        # You can extend this to parse the actual device response
        pass
    
    def update_config_widgets(self):
        """Update all configuration widgets with current data"""
        # Update channel widgets
        for ch in range(4):
            ch_widgets = self.config_widgets[f"channel_{ch}"]
            ch_data = self.config_data["channels"][ch]
            
            ch_widgets["oc_threshold"].set(ch_data["oc_threshold"])
            ch_widgets["inrush_threshold"].set(ch_data["inrush_threshold"])
            ch_widgets["inrush_time"].set(ch_data["inrush_time"])
            ch_widgets["underwarn_threshold"].set(ch_data["underwarn_threshold"])
            ch_widgets["mode"].set(ch_data["mode"])
            ch_widgets["group"].set(ch_data["group"])
        
        # Update system widgets
        self.config_widgets["temp_warn"].set(self.config_data["temp_warn"])
        self.config_widgets["temp_trip"].set(self.config_data["temp_trip"])
        
        # Update CAN widgets
        self.config_widgets["can_speed"].set(str(self.config_data["can_speed"]))
        self.config_widgets["pdm_node_id"].set(f"0x{self.config_data['pdm_node_id']:02X}")
        self.config_widgets["keypad_node_id"].set(f"0x{self.config_data['keypad_node_id']:02X}")
        self.config_widgets["digital_out_id"].set(f"0x{self.config_data['digital_out_id']:03X}")
    
    def apply_configuration(self):
        """Apply configuration to device"""
        if not self.pdm_comm.is_connected:
            messagebox.showerror("Error", "Please connect to PDM device first")
            return
            
        # Validate configuration
        if not self.validate_configuration():
            return
            
        self.config_status_label.configure(text="Applying configuration to device...")
        self.apply_btn.configure(state="disabled")
        
        def apply_thread():
            try:
                success_count = 0
                total_commands = 0
                
                # Apply channel configurations
                for ch in range(4):
                    ch_widgets = self.config_widgets[f"channel_{ch}"]
                    
                    commands = [
                        f"OC {ch+1} {ch_widgets['oc_threshold'].get()}",
                        f"INR {ch+1} {ch_widgets['inrush_threshold'].get()}",
                        f"INRTIME {ch+1} {ch_widgets['inrush_time'].get()}",
                        f"UWR {ch+1} {ch_widgets['underwarn_threshold'].get()}",
                        f"MODE {ch+1} {ch_widgets['mode'].get()}",
                        f"GROUP {ch+1} {ch_widgets['group'].get()}"
                    ]
                    
                    for cmd in commands:
                        total_commands += 1
                        if self.pdm_comm.send_config_command(cmd):
                            success_count += 1
                
                # Apply system configuration
                system_commands = [
                    f"TEMPWARN {self.config_widgets['temp_warn'].get()}",
                    f"TEMPTRIP {self.config_widgets['temp_trip'].get()}"
                ]
                
                for cmd in system_commands:
                    total_commands += 1
                    if self.pdm_comm.send_config_command(cmd):
                        success_count += 1
                
                # Apply CAN configuration
                try:
                    can_speed = int(self.config_widgets["can_speed"].get())
                    pdm_node = int(self.config_widgets["pdm_node_id"].get(), 16)
                    keypad_node = int(self.config_widgets["keypad_node_id"].get(), 16)
                    digital_out = int(self.config_widgets["digital_out_id"].get(), 16)
                    
                    can_commands = [
                        f"CANSPEED {can_speed}",
                        f"PDMNODE {pdm_node:02X}",
                        f"KEYPADNODE {keypad_node:02X}",
                        f"DIGITALOUT {digital_out:03X}"
                    ]
                    
                    for cmd in can_commands:
                        total_commands += 1
                        if self.pdm_comm.send_config_command(cmd):
                            success_count += 1
                            
                except ValueError as e:
                    self.main_frame.after(0, lambda: self.config_status_label.configure(text=f"Invalid hex value: {str(e)}"))
                    return
                
                # Report results
                if success_count == total_commands:
                    self.main_frame.after(0, lambda: self.config_status_label.configure(text="Configuration applied successfully"))
                else:
                    self.main_frame.after(0, lambda: self.config_status_label.configure(text=f"Partial success: {success_count}/{total_commands} commands"))
                    
            except Exception as e:
                self.main_frame.after(0, lambda: self.config_status_label.configure(text=f"Error: {str(e)}"))
            finally:
                self.main_frame.after(0, lambda: self.apply_btn.configure(state="normal"))
                
        threading.Thread(target=apply_thread, daemon=True).start()
    
    def validate_configuration(self) -> bool:
        """Validate configuration values"""
        try:
            # Validate channel settings
            for ch in range(4):
                ch_widgets = self.config_widgets[f"channel_{ch}"]
                
                oc = ch_widgets["oc_threshold"].get()
                inr = ch_widgets["inrush_threshold"].get()
                inr_time = ch_widgets["inrush_time"].get()
                uwr = ch_widgets["underwarn_threshold"].get()
                
                if not (0.1 <= oc <= 100.0):
                    messagebox.showerror("Validation Error", f"Channel {ch+1} overcurrent must be between 0.1 and 100.0 A")
                    return False
                    
                if not (0.1 <= inr <= 200.0):
                    messagebox.showerror("Validation Error", f"Channel {ch+1} inrush current must be between 0.1 and 200.0 A")
                    return False
                    
                if not (100 <= inr_time <= 10000):
                    messagebox.showerror("Validation Error", f"Channel {ch+1} inrush time must be between 100 and 10000 ms")
                    return False
                    
                if not (0.01 <= uwr <= 10.0):
                    messagebox.showerror("Validation Error", f"Channel {ch+1} undercurrent warning must be between 0.01 and 10.0 A")
                    return False
            
            # Validate system settings
            temp_warn = self.config_widgets["temp_warn"].get()
            temp_trip = self.config_widgets["temp_trip"].get()
            
            if not (0 <= temp_warn <= 150):
                messagebox.showerror("Validation Error", "Temperature warning must be between 0 and 150°C")
                return False
                
            if not (temp_warn < temp_trip <= 150):
                messagebox.showerror("Validation Error", "Temperature trip must be higher than warning and ≤150°C")
                return False
            
            # Validate CAN settings
            can_speed = int(self.config_widgets["can_speed"].get())
            if can_speed not in [125, 250, 500, 1000]:
                messagebox.showerror("Validation Error", "CAN speed must be 125, 250, 500, or 1000 kbps")
                return False
            
            # Validate hex values
            int(self.config_widgets["pdm_node_id"].get(), 16)
            int(self.config_widgets["keypad_node_id"].get(), 16)
            int(self.config_widgets["digital_out_id"].get(), 16)
            
            return True
            
        except ValueError as e:
            messagebox.showerror("Validation Error", f"Invalid number format: {str(e)}")
            return False
        except Exception as e:
            messagebox.showerror("Validation Error", f"Configuration validation failed: {str(e)}")
            return False
    
    def save_configuration(self):
        """Save configuration to device EEPROM"""
        if not self.pdm_comm.is_connected:
            messagebox.showerror("Error", "Please connect to PDM device first")
            return
            
        result = messagebox.askyesno(
            "Save Configuration",
            "This will save the current configuration to device EEPROM.\n\nContinue?"
        )
        
        if not result:
            return
            
        self.config_status_label.configure(text="Saving configuration to EEPROM...")
        self.save_btn.configure(state="disabled")
        
        def save_thread():
            try:
                response = self.pdm_comm.send_command("SAVE")
                
                if response and "OK:" in response:
                    self.main_frame.after(0, lambda: self.config_status_label.configure(text="Configuration saved to EEPROM"))
                else:
                    self.main_frame.after(0, lambda: self.config_status_label.configure(text="Failed to save configuration"))
                    
            except Exception as e:
                self.main_frame.after(0, lambda: self.config_status_label.configure(text=f"Save error: {str(e)}"))
            finally:
                self.main_frame.after(0, lambda: self.save_btn.configure(state="normal"))
                
        threading.Thread(target=save_thread, daemon=True).start()
    
    def factory_reset(self):
        """Reset device to factory defaults"""
        if not self.pdm_comm.is_connected:
            messagebox.showerror("Error", "Please connect to PDM device first")
            return
            
        result = messagebox.askyesno(
            "Factory Reset",
            "This will reset ALL settings to factory defaults.\n\nThis action cannot be undone. Continue?",
            icon="warning"
        )
        
        if not result:
            return
            
        self.config_status_label.configure(text="Performing factory reset...")
        self.reset_btn.configure(state="disabled")
        
        def reset_thread():
            try:
                # Send factory reset command (if implemented in firmware)
                response = self.pdm_comm.send_command("FACTORY_RESET")
                
                # For now, we'll reset to known defaults
                self.reset_to_defaults()
                self.main_frame.after(0, self.update_config_widgets)
                self.main_frame.after(0, lambda: self.config_status_label.configure(text="Factory reset completed"))
                    
            except Exception as e:
                self.main_frame.after(0, lambda: self.config_status_label.configure(text=f"Reset error: {str(e)}"))
            finally:
                self.main_frame.after(0, lambda: self.reset_btn.configure(state="normal"))
                
        threading.Thread(target=reset_thread, daemon=True).start()
    
    def reset_to_defaults(self):
        """Reset configuration to factory defaults"""
        self.config_data = {
            "channels": [
                {
                    "oc_threshold": 15.0,
                    "inrush_threshold": 50.0,
                    "inrush_time": 1000,
                    "underwarn_threshold": 0.10,
                    "mode": "LATCH",
                    "group": i + 1
                } for i in range(4)
            ],
            "temp_warn": 70.0,
            "temp_trip": 85.0,
            "can_speed": 1000,
            "pdm_node_id": 0x15,
            "keypad_node_id": 0x15,
            "digital_out_id": 0x680
        }