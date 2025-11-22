# QSafe v1.0.9 - Fix Trigger Detection Gravity Compensation

## What's Fixed

**Critical Bug Fix: ESP32 Trigger Detection Now Works Properly**

The ESP32 was measuring gravity instead of seismic motion, causing trigger detection to fail. This release fixes the gravity compensation to properly detect earthquakes.

### Problem
- **RMS stuck at ~987 mg**: ESP32 was measuring static gravity (1g) instead of motion
- **Kurtosis stuck at 0.00**: No variance in constant gravity signal
- **Triggers not detecting**: Dashboard showed motion, but ESP32 didn't trigger

### Solution
- Added gravity compensation before trigger detection ([main.cpp:325-332](https://github.com/Cem512/QSafe/blob/v1.0.9/src/main.cpp#L325-L332))
- Removes ~985 mg gravity bias to center Z-axis at 0
- Aligns ESP32 processing with dashboard's approach

### Expected Results After Update
- ✅ **RMS**: ~0-10 mg at rest (actual motion, not gravity)
- ✅ **Kurtosis**: Properly detects impulsive seismic events
- ✅ **Trigger Detection**: ESP32 and dashboard now synchronized

## Files

- **qsafe-ota.bin** (~1.1 MB): Use this for OTA updates via MQTT
- **qsafe-merged.bin** (~2 MB): Use this for initial flashing via USB/web

## Installation

### OTA Update (Recommended)
Your ESP32 nodes will automatically check for this update. To trigger manually:

```bash
mosquitto_pub -h YOUR_BROKER -t "eew/cmd/YOUR_NODE_ID" -m '{"command":"update"}'
```

Or the node will check automatically on next heartbeat (every 60 seconds).

### USB Flash (If OTA fails)
1. Visit [ESP Web Tools](https://web.esphome.io/)
2. Click "Install" → "Choose File"
3. Select `qsafe-merged.bin`
4. Click "Install" and follow prompts

## Technical Details

- **Firmware version**: `1.0.9` (from `1.0.8`)
- **Change**: Gravity compensation in trigger detection
- **Files modified**: `src/main.cpp`, `src/config.h`
- **Binary size**: ~1,088 KB (OTA), ~2,048 KB (full flash)

---

🤖 Generated with [Claude Code](https://claude.com/claude-code)
