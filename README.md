# QSafe - Earthquake Early Warning Node

A sophisticated ESP32-based seismograph node for earthquake early warning systems.

## Features

- **ADXL345 Accelerometer** - High-precision 3-axis sensor with SPI interface
- **Smart Trigger Detection** - Multi-stage analysis (RMS, Kurtosis, FFT spectral filtering)
- **Auto-Calibration** - Gravity vector compensation with drift detection
- **Secure MQTT** - TLS-encrypted communication with HiveMQ Cloud
- **WiFi Provisioning** - Easy setup via captive portal (no hardcoded credentials!)
- **Dual-Core Processing** - Real-time sampling on Core 1, networking on Core 0
- **NTP Time Sync** - Microsecond-precision timestamps for multi-node triangulation

## Quick Start

### 1. Configure MQTT Credentials

**First time only:**
```bash
# Copy the example file
cp credentials.h.example credentials.h

# Edit credentials.h and add your HiveMQ credentials:
# MQTT_USERNAME and MQTT_PASSWORD
```

See [CREDENTIALS_SETUP.md](CREDENTIALS_SETUP.md) for details.

### 2. Build Firmware

In VS Code with PlatformIO:
- Click the **✓ Build** button in the bottom toolbar

This creates a single merged firmware file:
```
.pio/build/esp32dev/qsafe-merged.bin
```

### 3. Flash to ESP32

**Option A: ESP Web Tools (Easiest)**
1. Visit https://web.esphome.io/
2. Click "Install" → "Choose File"
3. Select `qsafe-merged.bin`
4. Connect to your ESP32 and flash

**Option B: PlatformIO**
- Click the **→ Upload** button in VS Code

**Option C: Command Line**
```bash
pio run --target upload
```

### 4. First Boot Setup

After flashing:
1. ESP32 creates WiFi AP: `EEW-Setup-XXXX`
2. Connect to it from your phone/laptop
3. Configuration portal opens automatically
4. Enter your WiFi credentials only
5. Save → ESP32 reboots and connects

**WiFi credentials are stored in ESP32 - no need to reconfigure!**
**MQTT credentials are embedded in firmware during build.**

## Hardware Setup

### Wiring (ESP32 DevKit + ADXL345)

| ADXL345 | ESP32 | Description |
|---------|-------|-------------|
| VCC | 3.3V | Power |
| GND | GND | Ground |
| CS | GPIO 5 | Chip Select |
| SDI/MOSI | GPIO 23 | SPI Data In |
| SDO/MISO | GPIO 19 | SPI Data Out |
| SCL/SCK | GPIO 18 | SPI Clock |

### Mounting
- Install on stable surface (concrete floor, wall mount)
- Level the sensor (will auto-calibrate, but < 45° tilt recommended)
- Avoid vibration sources (washing machines, HVAC, foot traffic)

## System Architecture

```
┌─────────────────────────────────────────────────────┐
│                   ESP32 (Dual Core)                 │
├─────────────────────────────────────────────────────┤
│  Core 1: Sampling Task (200 Hz)                     │
│  • Read ADXL345 via SPI                             │
│  • Apply calibration                                │
│  • Fill circular buffer (2000 samples = 10s)        │
│  • Trigger detection:                               │
│    - RMS > 3 mg threshold                           │
│    - Kurtosis > 5 (impulsiveness)                   │
│    - Spectral ratio > 2 (FFT-based noise rejection) │
│    - Duration > 500 ms                              │
├─────────────────────────────────────────────────────┤
│  Core 0: Main Loop                                  │
│  • WiFi connection management                       │
│  • MQTT TLS communication                           │
│  • NTP time synchronization                         │
│  • Heartbeat every 60s                              │
│  • Calibration drift checks (hourly)                │
└─────────────────────────────────────────────────────┘
           │
           ▼
     ┌───────────┐
     │  HiveMQ   │  MQTT Broker (TLS Port 8883)
     │  Cloud    │
     └───────────┘
```

## Configuration

Edit `src/config.h` for:
- Trigger thresholds
- Sample rate (default: 200 Hz)
- MQTT topics
- Calibration parameters
- Debug flags

## MQTT Topics

### Publishing (Node → Server)
- `eew/register` - Node registration on boot
- `eew/heartbeat/{node_id}` - Health status (every 60s)
- `eew/raw/{node_id}` - Trigger events with waveform data
- `eew/status/{node_id}` - Status updates

### Subscribing (Server → Node)
- `eew/alert` - Earthquake alerts from server
- `eew/cmd/{node_id}` - Remote commands

### Commands
```json
{"command": "calibrate"}  // Perform calibration
{"command": "restart"}    // Reboot node
{"command": "status"}     // Request immediate heartbeat
```

## LED Indicators

- **Solid On** - System ready
- **Fast Blink (200ms)** - Initialization error (check ADXL345)
- **Slow Blink (500ms)** - MQTT connection error (check credentials)
- **Triple Blink** - Earthquake trigger detected
- **Rapid Flash** - Earthquake alert received from server

## Serial Monitor

Connect at **115200 baud** to see:
```
╔══════════════════════════════════════════╗
║   EEW Node Firmware v1.0.0               ║
║   Earthquake Early Warning System        ║
╚══════════════════════════════════════════╝

[SETUP] Getting node identity...
[ID] Hardware ID: 24E124CE4F28D1A0
[ID] Node ID: Node_D1A0
[MQTT] ✓ Loaded credentials from NVS
[WiFi] ✓ Connected!
[ADXL345] ✓ Initialized successfully
[CAL] ✓ Using saved calibration
[MQTT] ✓ Connected!
[SETUP] ✓✓✓ SYSTEM READY ✓✓✓
```

## Files

- `src/main.cpp` - Main application logic
- `src/adxl345.cpp` - ADXL345 SPI driver
- `src/calibration.cpp` - Gravity compensation
- `src/trigger.cpp` - Signal processing & FFT
- `src/mqtt_handler.cpp` - MQTT TLS client
- `src/config.h` - All configuration parameters
- `merge_bins.py` - Build script (creates merged firmware)
- `FLASHING.md` - Detailed flashing instructions

## Troubleshooting

**ADXL345 initialization failed**
- Check wiring (use multimeter)
- Verify 3.3V power (not 5V!)
- Try lower SPI speed in code

**WiFi portal not appearing**
- Wait 30 seconds after boot
- Search for `EEW-Setup-XXXX` network
- Press RESET button on ESP32

**MQTT connection fails**
- Verify credentials in WiFi portal
- Check internet connectivity
- HiveMQ free tier has connection limits

**False triggers**
- Increase RMS threshold in config.h
- Mount sensor more rigidly
- Check for vibration sources nearby

**No triggers during actual shaking**
- Decrease RMS threshold
- Check sensor orientation (Z-axis should be vertical)
- Verify calibration (send `{"command":"calibrate"}`)

## Performance

- **Sampling Rate**: 200 Hz (5ms period)
- **Timing Accuracy**: ±1ms (FreeRTOS vTaskDelayUntil)
- **Memory Usage**: ~60KB heap, 4KB stack per task
- **Trigger Latency**: <500ms from event to MQTT publish
- **Power**: ~200mA @ 5V (WiFi active)

## Security

✓ **MQTT credentials** - Stored encrypted in NVS (not in source code)
✓ **TLS encryption** - All MQTT traffic encrypted
✓ **No hardcoded secrets** - Configured via WiFi portal
✓ **Certificate pinning** - HiveMQ Cloud root CA validated

## License

This project is for educational and research purposes.

## Credits

Built with:
- ESP32 Arduino Framework
- ArduinoFFT (signal processing)
- PubSubClient (MQTT)
- WiFiManager (provisioning)
- ArduinoJson (data serialization)

---

**Version**: 1.0.0
**Author**: QSafe Team
**Hardware**: ESP32 + ADXL345
