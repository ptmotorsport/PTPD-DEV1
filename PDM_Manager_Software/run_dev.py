#!/usr/bin/env python3
"""
Development startup script for PDM Manager
"""

import sys
import os

# Add project root to path
project_root = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, project_root)

# Import and run main
from main import main

if __name__ == "__main__":
    main()