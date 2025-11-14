#!/bin/bash
# QSafe EEW Server Installation Script
# For Raspberry Pi (Raspberry Pi OS)

echo "========================================"
echo "QSafe EEW Server - Installation"
echo "========================================"

# Get current user and home directory
CURRENT_USER=$(whoami)
HOME_DIR=$(eval echo ~$CURRENT_USER)

echo "Current user: $CURRENT_USER"
echo "Home directory: $HOME_DIR"

# Check Python version
python_version=$(python3 --version 2>&1 | awk '{print $2}')
echo "Python version: $python_version"

# Update system
echo ""
echo "[1/5] Updating system packages..."
sudo apt-get update
sudo apt-get install -y python3-pip python3-venv

# Create virtual environment in current directory
echo ""
echo "[2/5] Creating virtual environment..."
cd $HOME_DIR/dev-dashboard
python3 -m venv venv
source venv/bin/activate

# Install Python dependencies
echo ""
echo "[3/5] Installing Python packages..."
pip install --upgrade pip
pip install -r requirements.txt

# Configure MQTT credentials
echo ""
echo "[4/5] Configuration..."
echo "Development broker configured for local network"
echo "No authentication required (development only)"
echo ""

# Display network info
echo ""
echo "[5/5] Network information:"
LOCAL_IP=$(hostname -I | awk '{print $1}')
echo "  Raspberry Pi IP: $LOCAL_IP"
echo "  MQTT Broker: mqtt://$LOCAL_IP:1883"
echo "  Dashboard: http://$LOCAL_IP:5000"
echo ""

echo "========================================"
echo "Installation Complete!"
echo "========================================"
echo ""
echo "To start the dashboard:"
echo "  cd $HOME_DIR/dev-dashboard"
echo "  source venv/bin/activate"
echo "  python3 dashboard_server.py"
echo ""
echo "Then open browser: http://$LOCAL_IP:5000"
echo ""
