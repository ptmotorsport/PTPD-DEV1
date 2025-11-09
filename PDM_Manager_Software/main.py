#!/usr/bin/env python3
"""
PT Motorsport PDM Manager
Professional PDM configuration and monitoring software
"""

import sys
import os
import tkinter as tk
from tkinter import messagebox

# Add src directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'src'))

try:
    import customtkinter as ctk
    from gui.main_window import PDMManagerApp
except ImportError as e:
    messagebox.showerror(
        "Missing Dependencies", 
        f"Required library not found: {e}\n\n"
        "Please install dependencies:\n"
        "pip install -r requirements.txt"
    )
    sys.exit(1)

def main():
    """Main entry point"""
    try:
        # Set appearance mode and theme
        ctk.set_appearance_mode("light")  # Professional light theme
        ctk.set_default_color_theme("blue")
        
        # Create and run application
        app = PDMManagerApp()
        app.run()
        
    except Exception as e:
        messagebox.showerror("Application Error", f"Failed to start PDM Manager:\n{str(e)}")
        sys.exit(1)

if __name__ == "__main__":
    main()