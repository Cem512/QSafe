# Development Dashboard Setup Instructions

Complete guide to set up real-time seismograph visualization on your Raspberry Pi.

## Quick Start Summary

1. **On Raspberry Pi:** Install Mosquitto + Python dashboard
2. **On ESP32:** Flash firmware v1.0.8 with dual MQTT support
3. **Open browser:** Visit http://192.168.31.28:5000

---

## Part 1: Raspberry Pi Setup

### Step 1: Transfer Files

From Windows PowerShell or Command Prompt:

```powershell
# Option A: Using SCP (if OpenSSH is installed on Windows)
scp -r C:\Users\cemfi\seismograph-system\dev-dashboard pi@192.168.31.28:/home/pi/

# Option B: Copy to USB drive, then on Pi:
#   cp -r /media/usb/dev-dashboard /home/pi/
```

Or use **WinSCP** (GUI):
1. Download: https://winscp.net/
2. Connect to 192.168.31.28
3. Username: pi
4. Password: orhint35
5. Drag `dev-dashboard` folder to `/home/pi/`

### Step 2: SSH to Raspberry Pi

```powershell
ssh pi@192.168.31.28
# Password: orhint35
```

### Step 3: Run Installation Script

```bash
cd /home/pi/dev-dashboard
chmod +x setup_pi.sh
./setup_pi.sh
```

This will:
- Install Mosquitto MQTT broker (port 1883)
- Install Python 3 and dependencies
- Create virtual environment
- Configure Mosquitto for local dev (no auth)
- Test MQTT connection

Expected output:
```
✓ Mosquitto running on port 1883
✓ Python environment ready
✓ MQTT broker test successful
Raspberry Pi IP: 192.168.31.28
```

### Step 4: Start Dashboard Server

```bash
cd /home/pi/dev-dashboard
source venv/bin/activate
python3 dashboard_server.py
```

You should see:
```
QSafe Development Dashboard Server
Connected to MQTT broker
Dashboard available at: http://192.168.31.28:5000
```

**Keep this terminal open!** The server runs in foreground for now.

### Step 5: Open Dashboard in Browser

On your Windows PC, open a web browser and visit:

**http://192.168.31.28:5000**

You should see the dashboard with:
- Status: "Connecting to server..." → "Connected"
- Empty charts (no data yet, waiting for ESP32)

---

## Part 2: ESP32 Firmware Update

The ESP32 code has been updated to support dual MQTT:
- **Cloud broker (HiveMQ):** Triggers, heartbeats, status (unchanged)
- **Dev broker (Raspberry Pi):** All data + 200 Hz streaming

### Option A: Flash New Firmware via USB (Recommended)

1. In VS Code, open the QSafe project
2. Verify `config.h` has these settings:
   ```cpp
   #define ENABLE_DEV_BROKER     true
   #define MQTT_DEV_BROKER       "192.168.31.28"
   #define MQTT_DEV_PORT         1883
   ```
3. Click **Build** (✓)
4. Connect ESP32 via USB
5. Click **Upload** (→)

### Option B: OTA Update (If supported in future version)

Not yet available for v1.0.8 since it requires code changes.

### Verification

After flashing, open Serial Monitor (115200 baud):

```
[MQTT] Connecting to cloud broker...
[MQTT] ✓ Cloud broker connected
[MQTT] Connecting to dev broker...
[MQTT] ✓ Dev broker connected
```

**Important:** You should see **both** connections succeed!

If dev broker fails:
```
[MQTT] ✗ Dev connection failed, rc=-2
[MQTT] ⚠ Dev broker not available (continuing anyway)
```

This means the Pi is not reachable. Check:
- Pi and ESP32 on same WiFi network (192.168.31.x)
- Mosquitto is running on Pi: `sudo systemctl status mosquitto`
- Pi firewall allows port 1883: `sudo ufw allow 1883`

---

## Part 3: Testing the Dashboard

### What You Should See

Once ESP32 connects:

1. **Connection Status** (top bar)
   - Changes from "Connecting..." to "Connected"
   - Green pulsing indicator

2. **Real-time Acceleration Chart**
   - Three lines: X (red), Y (blue), Z (green)
   - Updates 20 times per second
   - Shows last ~5 seconds of data

3. **STA/LTA Chart**
   - Yellow line: Current STA/LTA ratio
   - Red dashed line: Trigger threshold (3.0)
   - Spikes when motion detected

4. **Node Statistics**
   - RMS Acceleration (mg)
   - Kurtosis
   - Peak Frequency (Hz)
   - STA/LTA Ratio

5. **Orientation**
   - Roll and Pitch (degrees)
   - Node ID
   - Trigger count

6. **Trigger Events Log**
   - Shows when triggers occur
   - Red alert banner when triggered
   - Scrollable history

### Test 1: Verify Streaming

1. Gently tap the ESP32 node
2. You should see spikes in the acceleration chart immediately
3. STA/LTA ratio should increase

### Test 2: Trigger an Event

1. Enable test mode (to bypass spectral filter):
   ```json
   Topic: eew/cmd/Node_XXXX
   Message: {"command": "testmode"}
   ```

2. Shake the node firmly for 1 second
3. You should see:
   - LED blinks 3 times on node
   - Red alert banner on dashboard
   - New entry in trigger log
   - "Triggers" counter increases

### Test 3: Verify Dual Publishing

The node should publish to **both** brokers:

**On HiveMQ Cloud Dashboard:**
- Check `eew/heartbeat/Node_XXXX` - should receive messages every 10s
- Check `eew/raw/Node_XXXX` - should receive triggers

**On Raspberry Pi:**
```bash
# Subscribe to all topics
mosquitto_sub -h localhost -t 'eew/#' -v
```

You should see:
```
eew/stream/Node_XXXX {"x": 5.2, "y": -1.3, "z": 1005.8, "timestamp_ms": 12345}
eew/heartbeat/Node_XXXX {...}
eew/raw/Node_XXXX {...}
```

The `eew/stream/*` topic only appears on local dev broker (not HiveMQ).

---

## Part 4: Auto-Start Dashboard (Optional)

To make dashboard start automatically on Pi boot:

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

Stop service:
```bash
sudo systemctl stop dev-dashboard
```

---

## Troubleshooting

### Dashboard shows "Disconnected"

**Check Flask server:**
```bash
ps aux | grep dashboard_server
```

If not running:
```bash
cd /home/pi/dev-dashboard
source venv/bin/activate
python3 dashboard_server.py
```

**Check firewall:**
```bash
sudo ufw status
# If active, allow port 5000:
sudo ufw allow 5000
```

### No data on charts

**Check ESP32 serial monitor:**
```
[MQTT] ✓ Dev broker connected  ← Should see this!
```

If dev broker fails:
```
[MQTT] ✗ Dev connection failed, rc=-2
```

Possible causes:
- ESP32 and Pi on different WiFi networks
- Wrong IP in config.h (should be 192.168.31.28)
- Mosquitto not running on Pi

**Check Mosquitto status:**
```bash
sudo systemctl status mosquitto
```

If not running:
```bash
sudo systemctl start mosquitto
```

**Test MQTT manually:**
```bash
# Terminal 1: Subscribe
mosquitto_sub -h 192.168.31.28 -t 'eew/stream/#' -v

# Terminal 2: Publish test
mosquitto_pub -h 192.168.31.28 -t 'eew/stream/test' -m '{"x":1,"y":2,"z":3}'
```

You should see the message appear in Terminal 1.

### Charts update slowly or freeze

This usually means:
- Network congestion (too much MQTT traffic)
- Pi overloaded (check with `top`)

The streaming is rate-limited to 20 Hz (every 10th sample of 200 Hz). You can adjust this in `dual_mqtt.cpp`:

```cpp
// Change from 10 to 20 for 10 Hz updates
if (_stream_counter < 20) {
    return;
}
```

### ESP32 won't flash

**Error:** "Failed to connect to ESP32: Timed out waiting for packet header"

Solution:
1. Hold BOOT button on ESP32
2. Click Upload in VS Code
3. Release BOOT when "Connecting..." appears

### MQTT buffer overflow

If you see:
```
[MQTT] ✗ Publish failed: buffer too small
```

Increase buffer in `dual_mqtt.cpp`:
```cpp
_cloud_mqtt.setBufferSize(16384);  // Increase to 32768 if needed
_dev_mqtt.setBufferSize(512);      // Increase to 1024 if needed
```

---

## Configuration Reference

### Mosquitto Config
**File:** `/etc/mosquitto/conf.d/development.conf`

```conf
# Development mode - NO SECURITY
listener 1883 0.0.0.0
allow_anonymous true
```

**Security Note:** This allows anyone on the network to publish. Fine for dev, **do NOT use in production!**

### ESP32 Config
**File:** `src/config.h`

```cpp
#define ENABLE_DEV_BROKER     true              // Enable dual MQTT
#define MQTT_DEV_BROKER       "192.168.31.28"   // Your Pi's IP
#define MQTT_DEV_PORT         1883              // Mosquitto port
```

To **disable** dev broker and use only HiveMQ:
```cpp
#define ENABLE_DEV_BROKER     false
```

### Dashboard Config
**File:** `dashboard_server.py`

```python
MQTT_BROKER = "192.168.31.28"  # Pi's IP
MQTT_PORT = 1883
STREAM_BUFFER_SIZE = 2000      # Show last 10 seconds
```

---

## Network Diagram

```
┌─────────────────────────────────────────┐
│  ESP32 Node (192.168.31.x)              │
│  ┌─────────────┐                        │
│  │ ADXL345     │ → 200 Hz sampling      │
│  │ Accelero    │                        │
│  └─────────────┘                        │
│         │                                │
│    ┌────┴────┐                          │
│    │Dual MQTT│                          │
│    └────┬────┘                          │
└─────────┼───────────────────────────────┘
          │
      ┌───┴───────────────────────────┐
      │                               │
      ▼                               ▼
┌──────────────┐             ┌────────────────┐
│ HiveMQ Cloud │             │ Raspberry Pi   │
│ (TLS:8883)   │             │ 192.168.31.28  │
│              │             │                │
│ • Triggers   │             │ Mosquitto:1883 │
│ • Heartbeat  │             │ • Stream (20Hz)│
│ • Status     │             │ • Triggers     │
│              │             │ • Heartbeat    │
└──────────────┘             │                │
                             │ Flask:5000     │
                             │ • Dashboard    │
                             │ • Real-time UI │
                             └────────────────┘
                                      │
                                      ▼
                            ┌──────────────────┐
                            │ Web Browser      │
                            │ http://.28:5000  │
                            │                  │
                            │ • Live charts    │
                            │ • Node stats     │
                            │ • Trigger log    │
                            └──────────────────┘
```

---

## Next Steps

After successful setup:

1. **Characterize your node:**
   - Place on stable surface
   - Observe baseline RMS and orientation
   - Note typical noise levels

2. **Test sensitivity:**
   - Tap gently → Should see in charts, not trigger
   - Tap firmly → Should trigger (if test mode on)
   - Jump nearby → Should see P/S wave patterns

3. **Optimize thresholds:**
   - Adjust `RMS_THRESHOLD_MG` in config.h
   - Tune `TRIGGER_RATIO` in sta_lta_picker.py
   - Balance: too sensitive = false alarms, too high = miss events

4. **Production deployment:**
   - Disable test mode: `{"command": "testmode"}` (toggle off)
   - Set `ENABLE_DEV_BROKER false` (optional, to reduce network load)
   - Keep HiveMQ for remote monitoring

---

**Questions or issues?**
Check the README.md files in each directory, or review the troubleshooting section above.

Good luck! 🌍📊
