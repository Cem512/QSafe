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
- **🆕 GitHub Auto-OTA** - Automatic firmware updates from GitHub Releases (zero-touch!)
- **🆕 CI/CD Pipeline** - Automated firmware builds via GitHub Actions

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

This creates two firmware files:
```
.pio/build/esp32dev/qsafe-ota.bin      (~1 MB - for OTA updates)
.pio/build/esp32dev/qsafe-merged.bin   (~2 MB - for initial flash)
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
{"command": "update"}     // Check for and install OTA updates
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
║   EEW Node Firmware v1.0.1               ║
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

---

## 🔄 Automatic GitHub OTA Updates

**Your ESP32 nodes can now update themselves automatically when you publish new releases on GitHub!**

### System Architecture

```
┌─────────────────────────────────────────────────────────┐
│  Developer Workflow                                     │
├─────────────────────────────────────────────────────────┤
│  1. Make code changes                                   │
│  2. git commit -m "Add feature"                         │
│  3. git tag v1.2.0                                      │
│  4. git push origin v1.2.0                              │
└──────────────────┬──────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────────────┐
│  GitHub Actions (.github/workflows/build-firmware.yml)  │
├─────────────────────────────────────────────────────────┤
│  • Triggered by tag push (v*.*.*)                       │
│  • Installs PlatformIO                                  │
│  • Updates version in src/config.h                      │
│  • Builds firmware for esp32dev                         │
│  • Creates:                                             │
│    - qsafe-ota.bin (~1.1 MB)                            │
│    - qsafe-esp32-{version}.bin (backup)                 │
│    - firmware-metadata.json (MD5, size, date)           │
│  • Creates GitHub Release with binaries                 │
│  • Build time: ~2-3 minutes                             │
└──────────────────┬──────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────────────┐
│  GitHub Releases (api.github.com/repos/.../releases)    │
├─────────────────────────────────────────────────────────┤
│  Release: v1.2.0                                        │
│  Assets:                                                │
│    • qsafe-ota.bin                                      │
│    • qsafe-esp32-1.2.0.bin                              │
│    • firmware-metadata.json                             │
└──────────────────┬──────────────────────────────────────┘
                   │
                   │ (ESP32 checks every 24 hours)
                   │
                   ▼
┌─────────────────────────────────────────────────────────┐
│  ESP32 Node (ota_updater.cpp)                           │
├─────────────────────────────────────────────────────────┤
│  1. WiFiClientSecure connects to api.github.com         │
│  2. GET /repos/Cem512/QSafe/releases/latest             │
│  3. Parse JSON response (tag_name, assets)              │
│  4. Compare versions (current: 1.0.9 vs latest: 1.2.0)  │
│  5. If newer: Download qsafe-ota.bin via HTTPS          │
│  6. Verify MD5 checksum from metadata                   │
│  7. Write to OTA partition (Update.writeStream)         │
│  8. Verify flash integrity (Update.end)                 │
│  9. Reboot with new firmware (ESP.restart)              │
│  10. Rollback protection (only update if newer)         │
└─────────────────────────────────────────────────────────┘
```

### How It Works

1. **You push code** to GitHub with a version tag (e.g., `v1.2.0`)
2. **GitHub Actions** automatically builds the firmware
3. **ESP32 nodes** check GitHub every 24 hours for updates
4. **Automatic download & install** - No manual intervention needed!

### Quick Setup

```bash
# 1. Make sure you have the latest code
git pull

# 2. When ready to release:
git add .
git commit -m "Your changes here"
git tag v1.2.0
git push origin main
git push origin v1.2.0

# 3. GitHub Actions builds it automatically
# 4. Your ESP32s update themselves within 24 hours!
```

### Manual Build & Release

If you prefer to build and upload firmware manually:

```bash
# Run the automated build script
./build_and_prepare_release.bat

# This creates:
# - qsafe-ota.bin (~1 MB)
# - qsafe-merged.bin (~2 MB)
# - firmware-metadata.json (with MD5 hash)

# Then upload these files to your GitHub release
```

### Configuration

The GitHub OTA system is already configured for the **Cem512/QSafe** repository. ESP32 nodes check for updates every 24 hours by default.

To change update frequency, edit `src/ota_updater.cpp`:
```cpp
// Check every 12 hours instead of 24
#define OTA_CHECK_INTERVAL_MS (12 * 60 * 60 * 1000)
```

### Monitoring Updates

Connect via serial monitor (115200 baud) to see:
```
[OTA] Checking for firmware updates...
[OTA] Current version: 1.0.9
[OTA] Latest version: 1.1.0
[OTA] ✓ New version available!
[OTA] Found firmware: qsafe-ota.bin
[OTA] MD5: a3d5f8e9c1b2a4d6e7f8a9b0c1d2e3f4
[OTA] Downloading firmware...
[OTA] Firmware size: 1126400 bytes
[OTA] ✓ Firmware verified successfully!
[OTA] ✓ Update successful! Rebooting...
```

### Documentation

- **[GITHUB_OTA_GUIDE.md](GITHUB_OTA_GUIDE.md)** - Complete setup guide and troubleshooting
- **[QUICK_START_OTA.md](QUICK_START_OTA.md)** - 5-minute quick start
- **[CHANGELOG_GITHUB_OTA.md](CHANGELOG_GITHUB_OTA.md)** - Implementation details

---

## Files

### Source Code
- `src/main.cpp` - Main application logic
- `src/adxl345.cpp` - ADXL345 SPI driver
- `src/calibration.cpp` - Gravity compensation
- `src/trigger.cpp` - Signal processing & FFT
- `src/mqtt_handler.cpp` - MQTT TLS client
- `src/ota_updater.cpp` - **GitHub OTA firmware update manager**
- `src/config.h` - All configuration parameters

### Build & Deployment
- `.github/workflows/build-firmware.yml` - **GitHub Actions CI/CD pipeline**
- `build_and_prepare_release.bat` - **Automated build script for releases**
- `merge_bins.py` - Creates OTA and merged firmware binaries
- `platformio.ini` - PlatformIO build configuration

### Documentation
- `FLASHING.md` - Detailed flashing instructions
- `OTA_UPDATE_GUIDE.md` - Original OTA documentation (MQTT-based)
- `GITHUB_OTA_GUIDE.md` - **GitHub Auto-OTA complete guide**
- `QUICK_START_OTA.md` - **Quick start for GitHub OTA**
- `CHANGELOG_GITHUB_OTA.md` - **GitHub OTA implementation details**
- `CREDENTIALS_SETUP.md` - MQTT credentials configuration

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

**Version**: 1.1.0
**Author**: QSafe Team
**Hardware**: ESP32 + ADXL345

## Updates & Changelog

### v1.1.0 (Latest) - **GitHub Auto-OTA & CI/CD**
**Released**: 2025-11-22

**🎉 Major New Features:**
- ✅ **Automatic GitHub OTA Updates** - ESP32 nodes now check GitHub for firmware updates every 24 hours and update themselves automatically
- ✅ **GitHub Actions CI/CD** - Automated firmware builds on every release tag
- ✅ **Build Automation** - `build_and_prepare_release.bat` script for manual builds
- ✅ **Version Management** - Centralized version tracking via `src/config.h`
- ✅ **MD5 Verification** - Firmware integrity checks during OTA updates
- ✅ **Comprehensive Documentation** - Added GITHUB_OTA_GUIDE.md, QUICK_START_OTA.md, CHANGELOG_GITHUB_OTA.md

**Technical Implementation:**
- GitHub Actions workflow builds firmware on tag push (e.g., `v1.2.0`)
- Creates `qsafe-ota.bin` and `qsafe-merged.bin` with metadata
- ESP32 queries GitHub API (`api.github.com/repos/Cem512/QSafe/releases/latest`)
- Downloads firmware via HTTPS with MD5 verification
- Automatic rollback protection (only updates to newer versions)

**Files Added:**
- `.github/workflows/build-firmware.yml` - CI/CD pipeline
- `build_and_prepare_release.bat` - Manual build automation
- `GITHUB_OTA_GUIDE.md` - Complete OTA documentation
- `QUICK_START_OTA.md` - Quick reference guide
- `CHANGELOG_GITHUB_OTA.md` - Implementation details

**Configuration:**
- OTA check interval: 24 hours (configurable)
- Repository: Cem512/QSafe (pre-configured)
- Update method: GitHub Releases API

**Developer Notes:**
- Existing `ota_updater.cpp` already had GitHub OTA functionality
- Added GitHub Actions workflow for automated builds
- Manual builds still supported via PlatformIO
- Zero-touch updates for deployed nodes

### v1.0.9 - **Critical Bug Fix**
**Released**: 2025-11-15

**What's Fixed:**
- **Critical**: Fixed ESP32 trigger detection gravity compensation
  - Problem: ESP32 was measuring static gravity (~987 mg) instead of motion
  - Solution: Added gravity compensation before trigger detection
  - Impact: RMS now shows actual motion (~5-15 mg) instead of gravity
  - Result: Trigger detection now works properly with dashboard

**Technical Details:**
- Modified `src/main.cpp` to remove ~985 mg gravity bias before `addSample()`
- Aligns ESP32 processing with dashboard's gravity compensation approach
- RMS and Kurtosis calculations now measure actual seismic motion
- Trigger detection thresholds now function as designed

**Before this fix:**
- RMS stuck at ~987 mg (measuring gravity)
- Kurtosis stuck at 0.00 (no variance in constant signal)
- Trigger detection completely non-functional

**After this fix:**
- RMS: ~5-15 mg at rest (actual motion detection)
- Kurtosis: Properly detects impulsive events
- Trigger detection: Fully operational and synchronized with dashboard

### v1.0.8
- Enhanced OTA update system
- Added test mode for trigger detection (`{"command":"testmode"}`)
- Improved spectral filtering for noise rejection
- Added P-wave and S-wave classification

### v1.0.1
- Fixed OTA binary size issue (was 16 MB, now correctly ~1 MB)
- Improved merge script to use `firmware.bin` instead of `firmware.elf`
- Added file size verification during build
- Enhanced build process reliability

### v1.0.0
- Initial release
- Full seismograph functionality
- MQTT TLS communication
- Auto-calibration
- WiFi provisioning
