# ⚡ QSafe Quick Start - 4 Steps to Deploy

## Step 0: Setup Credentials 🔐

**First time only:**

1. Copy `credentials.h.example` to `credentials.h`
2. Edit `credentials.h`:
   ```cpp
   #define MQTT_USERNAME "your_hivemq_username"
   #define MQTT_PASSWORD "your_hivemq_password"
   ```
3. Save the file

📖 See [CREDENTIALS_SETUP.md](CREDENTIALS_SETUP.md) for details

---

## Step 1: Build 🔨

In VS Code:
1. Open the QSafe project folder
2. Click the **✓** (checkmark) icon in the bottom toolbar

**Result**: Creates `qsafe-merged.bin` in `.pio/build/esp32dev/`

---

## Step 2: Flash 📤

### Using ESP Web Tools (No Software Install Needed!)

1. **Visit**: https://web.esphome.io/
2. **Click**: "Install" → "Choose File"
3. **Select**: `.pio/build/esp32dev/qsafe-merged.bin`
4. **Click**: "Install"
5. **Connect**: Select your ESP32's COM port when prompted
6. **Wait**: 2-3 minutes for flashing

✅ **Done!** ESP32 is now programmed.

---

## Step 3: Configure 🔧

1. **ESP32 boots** and creates WiFi network: `EEW-Setup-XXXX`
2. **Connect** to this network from your phone/laptop
3. **Portal opens** automatically (or visit http://192.168.4.1)
4. **Fill in** WiFi credentials only:
   ```
   WiFi SSID:       YourWiFiName
   WiFi Password:   YourWiFiPassword
   ```
5. **Click**: Save
6. **ESP32 reboots** and connects to your WiFi

✅ **System Ready!** LED turns solid on.

**Note:** MQTT credentials were embedded during firmware build - end users don't need them!

---

## Verification ✓

Open Serial Monitor (115200 baud) and you should see:

```
╔══════════════════════════════════════════╗
║   EEW Node Firmware v1.0.0               ║
║   Earthquake Early Warning System        ║
╚══════════════════════════════════════════╝

[WiFi] ✓ Connected!
[ADXL345] ✓ Initialized successfully
[CAL] ✓ Using saved calibration
[MQTT] ✓ Connected!

[SETUP] ✓✓✓ SYSTEM READY ✓✓✓
```

---

## Need Help?

- **Full Guide**: See [FLASHING.md](FLASHING.md)
- **Documentation**: See [README.md](README.md)
- **Troubleshooting**: Check "Troubleshooting" section in README

---

## Hardware Checklist

Before flashing, verify wiring:

| ADXL345 Pin | ESP32 Pin |
|-------------|-----------|
| VCC         | 3.3V      |
| GND         | GND       |
| CS          | GPIO 5    |
| MOSI (SDI)  | GPIO 23   |
| MISO (SDO)  | GPIO 19   |
| SCK (SCL)   | GPIO 18   |

⚠️ **Important**: Use 3.3V, not 5V!

---

**That's it!** Your earthquake early warning node is deployed. 🌍
