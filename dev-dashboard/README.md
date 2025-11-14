# QSafe Development Dashboard

Real-time visualization dashboard for seismograph node development and debugging.

## Features

- **Real-time 3-axis acceleration plot** (200 Hz streaming)
- **STA/LTA ratio visualization** with trigger threshold
- **Node statistics display** (RMS, Kurtosis, Peak Frequency)
- **Orientation monitoring** (Roll, Pitch)
- **Trigger event log** with visual alerts
- **WebSocket-based updates** (low latency)

## Architecture

```
ESP32 Node (192.168.31.x)
    ├─ MQTT Topic: eew/stream/{node_id}  → High-frequency data (200 Hz)
    ├─ MQTT Topic: eew/heartbeat/{node_id} → Node statistics
    └─ MQTT Topic: eew/raw/{node_id}     → Trigger events

Raspberry Pi (192.168.31.28)
    ├─ Mosquitto MQTT Broker (port 1883)
    ├─ Flask + SocketIO Server (port 5000)
    └─ Web Dashboard (http://192.168.31.28:5000)
```

## Installation

### 1. Copy files to Raspberry Pi

From Windows:
```bash
scp -r C:\Users\cemfi\seismograph-system\dev-dashboard pi@192.168.31.28:/home/pi/
```

Or use WinSCP/FileZilla GUI.

### 2. Run setup script

SSH to Raspberry Pi:
```bash
ssh pi@192.168.31.28
```

Then run:
```bash
cd /home/pi/dev-dashboard
chmod +x setup_pi.sh
./setup_pi.sh
```

This will:
- Install Mosquitto MQTT broker
- Install Python dependencies
- Configure local MQTT broker (no authentication for dev)
- Test MQTT connection

### 3. Start dashboard server

```bash
cd /home/pi/dev-dashboard
source venv/bin/activate
python3 dashboard_server.py
```

### 4. Open dashboard in browser

Visit: **http://192.168.31.28:5000**

## Configuration

### Mosquitto MQTT Broker

Config file: `/etc/mosquitto/conf.d/development.conf`

```conf
# Development mode - NO SECURITY (local network only!)
listener 1883 0.0.0.0
allow_anonymous true
```

**Note:** This is for development only. Do not expose port 1883 to the internet!

### Dashboard Server

Edit `dashboard_server.py` if needed:

```python
MQTT_BROKER = "192.168.31.28"  # Local Raspberry Pi
MQTT_PORT = 1883
STREAM_BUFFER_SIZE = 2000  # 10 seconds @ 200 Hz
```

## MQTT Topics

The ESP32 node should publish to:

### 1. High-frequency streaming (200 Hz)
**Topic:** `eew/stream/{node_id}`

```json
{
  "x": -5.2,
  "y": 12.1,
  "z": 1005.3,
  "timestamp_ms": 1234567890
}
```

### 2. Heartbeat (every 10 seconds)
**Topic:** `eew/heartbeat/{node_id}`

```json
{
  "node_id": "Node_EFD0",
  "trigger_info": {
    "rms_mg": 8.5,
    "kurtosis": 3.2,
    "peak_freq_hz": 4.8
  },
  "orientation": {
    "roll_deg": 2,
    "pitch_deg": -1
  }
}
```

### 3. Trigger events
**Topic:** `eew/raw/{node_id}`

(Existing format - same as HiveMQ)

## Running as a Service

To auto-start on boot:

```bash
sudo tee /etc/systemd/system/dev-dashboard.service > /dev/null <<EOF
[Unit]
Description=QSafe Development Dashboard
After=network.target mosquitto.service

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/dev-dashboard
ExecStart=/home/pi/dev-dashboard/venv/bin/python3 /home/pi/dev-dashboard/dashboard_server.py
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable dev-dashboard
sudo systemctl start dev-dashboard
```

View logs:
```bash
sudo journalctl -u dev-dashboard -f
```

## Troubleshooting

**Dashboard shows "Disconnected":**
- Check Mosquitto is running: `sudo systemctl status mosquitto`
- Check Python server is running: `ps aux | grep dashboard_server`
- Check firewall: `sudo ufw status`

**No data on charts:**
- Verify ESP32 is publishing to local broker
- Check MQTT messages: `mosquitto_sub -h 192.168.31.28 -t 'eew/#' -v`
- Check ESP32 serial monitor for errors

**Mosquitto won't start:**
- Check config syntax: `mosquitto -c /etc/mosquitto/mosquitto.conf -v`
- Check port 1883 not in use: `sudo netstat -tulpn | grep 1883`

## Development vs Production

**Development (this setup):**
- Local MQTT broker on Pi
- No TLS encryption
- No authentication
- Full 200 Hz data streaming
- Web dashboard for debugging

**Production (HiveMQ Cloud):**
- Cloud MQTT broker
- TLS encryption
- Username/password auth
- Only trigger events (no streaming)
- Mobile app alerts

Both can run simultaneously! Node can publish to both brokers.

## Files

- `dashboard_server.py` - Flask + SocketIO server
- `templates/dashboard.html` - Web interface
- `setup_pi.sh` - Installation script
- `requirements.txt` - Python dependencies
- `README.md` - This file

---

**Version:** 1.0
**Platform:** Raspberry Pi OS (Debian-based)
**Python:** 3.7+
