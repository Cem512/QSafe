# QSafe Firmware Flashing Guide

## Method 1: ESP Web Tools (Easiest - Browser-Based) ⭐ RECOMMENDED

### Prerequisites
- Chrome, Edge, or Opera browser (required for Web Serial API)
- USB cable connected to ESP32

### Steps
1. **Build the firmware** in VS Code/PlatformIO (click ✓ checkmark icon)
   - This automatically creates: `.pio/build/esp32dev/qsafe-merged.bin`
2. Visit: **https://web.esphome.io/**
3. Click **"Install"** → **"Choose File"**
4. Select: **`.pio/build/esp32dev/qsafe-merged.bin`**
5. Click **"Install"** and follow the prompts
6. Click **"Connect"** and select your ESP32's COM port
7. Wait for flashing to complete (~2 minutes)

✓ **That's it!** Single file, no offsets needed!

---

## Method 2: Batch Script (Windows)

### Prerequisites
- Python with esptool installed: `pip install esptool`

### Steps
1. **Build the firmware** in VS Code/PlatformIO
2. Connect ESP32 to USB
3. Run: `flash_esp32.bat COM3` (replace COM3 with your port)

---

## Method 3: PlatformIO CLI (Recommended)

### Using VS Code
1. Click **Upload** button (→) in bottom toolbar

### Using Command Line
```bash
cd C:\Users\cemfi\Documents\PlatformIO\Projects\QSafe
pio run --target upload
```

---

## Method 4: esptool.py (Advanced)

```bash
python -m esptool --chip esp32 --port COM3 --baud 921600 \
  --before default_reset --after hard_reset write_flash -z \
  --flash_mode dio --flash_freq 40m --flash_size 4MB \
  0x1000 .pio/build/esp32dev/bootloader.bin \
  0x8000 .pio/build/esp32dev/partitions.bin \
  0x10000 .pio/build/esp32dev/firmware.bin
```

Replace `COM3` with your ESP32's serial port.

---

## Finding Your COM Port

### Windows
- Device Manager → Ports (COM & LPT)
- Look for "USB-SERIAL CH340" or "CP210x"

### Linux
```bash
ls /dev/ttyUSB*
```

### macOS
```bash
ls /dev/cu.usbserial-*
```

---

## First Boot Setup

After flashing:

1. **ESP32 creates WiFi AP**: `EEW-Setup-XXXX` (XXXX = last 4 MAC digits)
2. **Connect** to this network from phone/laptop
3. **Configuration portal** opens automatically at `192.168.4.1`
4. **Enter**:
   - WiFi SSID & Password
   - MQTT Username (HiveMQ credentials)
   - MQTT Password (HiveMQ credentials)
5. **Save** - ESP32 reboots and connects to WiFi
6. **Credentials saved** to NVS - no need to reconfigure

---

## Monitoring Serial Output

```bash
pio device monitor
```

Or use Arduino IDE Serial Monitor at **115200 baud**.

---

## Troubleshooting

### "Failed to connect to ESP32"
- Hold BOOT button while connecting
- Try lower baud rate: `--baud 115200`

### "Timed out waiting for packet header"
- Wrong COM port selected
- USB cable is power-only (not data)
- Driver not installed (install CH340 or CP210x driver)

### MQTT Connection Fails
- Verify HiveMQ credentials in WiFi portal
- Check broker address in `config.h`
- Ensure internet connectivity

---

## Security Note

✓ **FIXED**: Credentials are now stored securely in ESP32 NVS (non-volatile storage)
- No credentials in source code
- Configure via WiFi portal on first boot
- Encrypted storage in flash memory
