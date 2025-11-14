#!/bin/bash
# Development Dashboard Setup Script for Raspberry Pi
# Run this on your Raspberry Pi

set -e

echo "========================================"
echo "QSafe Development Dashboard Setup"
echo "========================================"

# Step 1: Install Mosquitto MQTT Broker
echo ""
echo "[1/5] Installing Mosquitto MQTT broker..."
sudo apt-get update
sudo apt-get install -y mosquitto mosquitto-clients

# Configure Mosquitto for local development (no authentication)
echo ""
echo "[2/5] Configuring Mosquitto..."
sudo tee /etc/mosquitto/conf.d/development.conf > /dev/null <<EOF
# Development configuration - NO SECURITY (local network only!)
listener 1883 0.0.0.0
allow_anonymous true
EOF

sudo systemctl restart mosquitto
sudo systemctl enable mosquitto

echo "✓ Mosquitto running on port 1883"

# Step 2: Install Python dependencies
echo ""
echo "[3/5] Installing Python packages..."
sudo apt-get install -y python3-pip python3-venv

# Create virtual environment
cd /home/pi
mkdir -p dev-dashboard
cd dev-dashboard

python3 -m venv venv
source venv/bin/activate

# Install web server dependencies
pip install --upgrade pip
pip install flask flask-socketio paho-mqtt numpy eventlet python-socketio

echo "✓ Python environment ready"

# Step 3: Test MQTT broker
echo ""
echo "[4/5] Testing MQTT broker..."
mosquitto_pub -h localhost -t test/hello -m "Mosquitto is working!" &
sleep 1
mosquitto_sub -h localhost -t test/hello -C 1

echo "✓ MQTT broker test successful"

# Step 4: Display network info
echo ""
echo "[5/5] Network information:"
echo "  Raspberry Pi IP: $(hostname -I | awk '{print $1}')"
echo "  MQTT Broker: mqtt://$(hostname -I | awk '{print $1}'):1883"
echo ""

echo "========================================"
echo "Setup Complete!"
echo "========================================"
echo ""
echo "Next steps:"
echo "  1. Copy dashboard files to /home/pi/dev-dashboard/"
echo "  2. Run: python3 dashboard_server.py"
echo "  3. Open browser: http://$(hostname -I | awk '{print $1}'):5000"
echo ""
