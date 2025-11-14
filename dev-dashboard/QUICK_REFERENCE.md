# QSafe Development Dashboard - Quick Reference

## 🚀 First Time Setup (5 minutes)

### 1. Transfer to Pi
```bash
scp -r C:\Users\cemfi\seismograph-system\dev-dashboard pi@192.168.31.28:/home/pi/
```

### 2. Install on Pi
```bash
ssh pi@192.168.31.28
cd /home/pi/dev-dashboard
chmod +x setup_pi.sh
./setup_pi.sh
```

### 3. Start Dashboard
```bash
cd /home/pi/dev-dashboard
source venv/bin/activate
python3 dashboard_server.py
```

### 4. Open Browser
http://192.168.31.28:5000

### 5. Flash ESP32
- Open QSafe project in VS Code
- Build → Upload

---

## 📡 MQTT Topics

### Production (HiveMQ + Pi)
```
eew/heartbeat/{node_id}  - Every 10s
eew/raw/{node_id}        - Trigger events
eew/status/{node_id}     - Command responses
```

### Development Only (Pi)
```
eew/stream/{node_id}     - 20 Hz acceleration stream
```

---

## 🔧 Common Commands

### Raspberry Pi

**Check Mosquitto status:**
```bash
sudo systemctl status mosquitto
```

**Start/Stop Mosquitto:**
```bash
sudo systemctl start mosquitto
sudo systemctl stop mosquitto
```

**Monitor MQTT messages:**
```bash
mosquitto_sub -h localhost -t 'eew/#' -v
```

**Check dashboard is running:**
```bash
ps aux | grep dashboard_server
```

**View dashboard logs (if using systemd):**
```bash
sudo journalctl -u dev-dashboard -f
```

### ESP32

**Enable test mode (bypass spectral filter):**
```json
Topic: eew/cmd/Node_XXXX
{"command": "testmode"}
```

**Trigger immediate heartbeat:**
```json
{"command": "status"}
```

**Calibrate now:**
```json
{"command": "calibrate"}
```

**Restart node:**
```json
{"command": "restart"}
```

---

## 🐛 Quick Troubleshooting

| Problem | Solution |
|---------|----------|
| Dashboard shows "Disconnected" | Check: `ps aux \| grep dashboard_server` |
| No data on charts | Check ESP32 serial: Should see "Dev broker connected" |
| ESP32 won't connect to dev broker | Verify IP in `config.h` is `192.168.31.28` |
| Charts freeze | Reduce stream rate in `dual_mqtt.cpp` |
| Mosquitto won't start | Check: `sudo systemctl status mosquitto` |

---

## 📊 What You Should See

### Dashboard
- ✅ "Connected" status (green dot)
- ✅ Three-axis acceleration chart updating smoothly
- ✅ STA/LTA ratio chart
- ✅ Node statistics (RMS, Kurtosis, etc.)
- ✅ Orientation (Roll, Pitch)

### ESP32 Serial Monitor
```
[MQTT] ✓ Cloud broker connected
[MQTT] ✓ Dev broker connected
```

### Test: Tap the Node
- Charts spike immediately
- STA/LTA increases
- No trigger (unless test mode enabled)

### Test: Firm Shake (with test mode)
- LED blinks 3× on node
- Red alert banner on dashboard
- New entry in trigger log

---

## ⚙️ Configuration Files

### Pi: Mosquitto Config
`/etc/mosquitto/conf.d/development.conf`
```conf
listener 1883 0.0.0.0
allow_anonymous true
```

### Pi: Dashboard Config
`/home/pi/dev-dashboard/dashboard_server.py`
```python
MQTT_BROKER = "192.168.31.28"
MQTT_PORT = 1883
```

### ESP32: Enable/Disable Dev Mode
`src/config.h`
```cpp
#define ENABLE_DEV_BROKER  true   // false to disable
#define MQTT_DEV_BROKER    "192.168.31.28"
```

---

## 🔐 Security Notes

**⚠️ WARNING:** Development setup has NO SECURITY!
- Mosquitto allows anonymous connections
- No encryption (HTTP, not HTTPS)
- Anyone on network can publish

**Only use on trusted local networks!**

For production, HiveMQ Cloud has TLS + authentication.

---

## 📈 Performance

### Network Usage (per node)
- Dev streaming: ~40 KB/s
- Production: ~1 KB/s

### Pi 4 Capacity
- 5-10 nodes streaming simultaneously
- Reduce rate for more nodes

### Browser
- Works best in Chrome/Edge/Firefox
- May lag in older browsers

---

## 🛠️ Advanced

### Auto-Start Dashboard on Boot
```bash
sudo systemctl enable dev-dashboard
sudo systemctl start dev-dashboard
```

### Change Stream Rate
Edit `src/dual_mqtt.cpp`:
```cpp
// Current: 200 Hz → 20 Hz (every 10th sample)
if (_stream_counter < 10) return;

// Change to 200 Hz → 10 Hz (every 20th sample)
if (_stream_counter < 20) return;
```

### Disable Dev Mode
Set in `src/config.h`:
```cpp
#define ENABLE_DEV_BROKER false
```

Then rebuild and flash ESP32.

---

## 📞 Support

**Full documentation:**
- `SETUP_INSTRUCTIONS.md` - Step-by-step guide
- `README.md` - Complete reference
- `FILES_SUMMARY.txt` - What each file does

**Serial monitor not working?**
- Baud rate: 115200
- Line ending: Both NL & CR

**Need to re-flash ESP32?**
1. Hold BOOT button
2. Click Upload
3. Release when "Connecting..." appears

---

## ✅ Success Checklist

Before asking for help, verify:

- [ ] Mosquitto running: `sudo systemctl status mosquitto`
- [ ] Dashboard running: `ps aux | grep dashboard_server`
- [ ] ESP32 shows dev broker connected in serial
- [ ] Pi and ESP32 on same network (192.168.31.x)
- [ ] Browser can reach Pi: `ping 192.168.31.28`
- [ ] Firewall allows 1883 and 5000: `sudo ufw status`

---

**Dashboard URL:** http://192.168.31.28:5000

**Good luck! 🌍📊**
