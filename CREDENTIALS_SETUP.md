# MQTT Credentials Setup Guide

## Overview

MQTT credentials are **compiled into the firmware** but **not stored in Git**. This approach:
- ✅ Keeps credentials separate from WiFi setup (end users don't need them)
- ✅ Prevents accidental credential exposure in version control
- ✅ Allows different credentials per deployment without code changes
- ✅ Embeds credentials securely in the firmware binary

---

## Setup Steps

### 1. Edit `credentials.h`

Open `credentials.h` in the project root and replace the placeholder values:

```cpp
#define MQTT_USERNAME "your_actual_username"
#define MQTT_PASSWORD "your_actual_password"
```

Example for HiveMQ Cloud:
```cpp
#define MQTT_USERNAME "node_user_001"
#define MQTT_PASSWORD "SecureP@ssw0rd!2024"
```

### 2. Build Firmware

The credentials will be compiled into the firmware binary:

```bash
# In VS Code: Click ✓ (Build button)
# Or via command line:
pio run
```

### 3. Flash to ESP32

The credentials are now **embedded in the firmware**:

```bash
# Using ESP Web Tools:
# Upload .pio/build/esp32dev/qsafe-merged.bin

# Or using PlatformIO:
pio run --target upload
```

---

## Security Notes

### ✅ What's Protected

- `credentials.h` is in `.gitignore` and will **never** be committed
- Only `credentials.h.example` (with placeholder values) is in Git
- Each developer/deployment can have different credentials

### ⚠️ Security Considerations

**Firmware Binary Contains Credentials**
- The `.bin` file contains your credentials in compiled form
- **Do not share** `.pio/build/` directory or `.bin` files publicly
- Consider `.bin` files as sensitive as credential files

**Alternative for Higher Security**
If you need maximum security (credentials not in firmware):
- Use Option 2 below (Environment Variables)
- Or Option 3 (OTA with TLS client certificates)

---

## Alternative Approaches

### Option 2: Environment Variables (Recommended for CI/CD)

Use platformio build flags with environment variables:

**platformio.ini:**
```ini
[env:esp32dev]
build_flags =
    -DMQTT_USERNAME=\"${sysenv.MQTT_USER}\"
    -DMQTT_PASSWORD=\"${sysenv.MQTT_PASS}\"
```

**Before building:**
```bash
# Windows
set MQTT_USER=your_username
set MQTT_PASS=your_password
pio run

# Linux/Mac
export MQTT_USER=your_username
export MQTT_PASS=your_password
pio run
```

Then remove `#include "../credentials.h"` from `config.h`.

---

### Option 3: Remote Configuration API

Fetch credentials from a secure API at runtime:

**Pros:**
- Credentials never in firmware
- Can rotate credentials remotely
- Centralized credential management

**Cons:**
- Requires internet connection before MQTT
- More complex implementation
- Need secure API endpoint

**Implementation:**
```cpp
// Pseudo-code
void setup() {
    connectWiFi();
    String creds = fetchFromAPI("https://api.yourserver.com/node/credentials");
    parseCredentials(creds);
    connectMQTT();
}
```

---

### Option 4: Hardware Security Module (HSM)

Use ESP32 secure boot + flash encryption:

**Pros:**
- Maximum security
- Credentials encrypted in flash
- Tamper-resistant

**Cons:**
- Complex setup
- One-time programmable (OTP) fuses
- Difficult to update

**Setup:**
```bash
# Enable flash encryption (irreversible!)
esptool.py burn_efuse FLASH_CRYPT_CNT
esptool.py burn_efuse FLASH_CRYPT_CONFIG 0x0F
```

---

## Multi-Deployment Workflow

For deploying multiple nodes with different credentials:

### Method A: Build Per Node

```bash
# Node 1
echo "MQTT_USERNAME=node1_user" > credentials.h
pio run
cp .pio/build/esp32dev/qsafe-merged.bin node1.bin

# Node 2
echo "MQTT_USERNAME=node2_user" > credentials.h
pio run
cp .pio/build/esp32dev/qsafe-merged.bin node2.bin
```

### Method B: Build Script

Create `build_multi.sh`:
```bash
#!/bin/bash
for node in node1 node2 node3; do
    echo "#define MQTT_USERNAME \"$node\"" > credentials.h
    echo "#define MQTT_PASSWORD \"pass_$node\"" >> credentials.h
    pio run
    cp .pio/build/esp32dev/qsafe-merged.bin "firmware_$node.bin"
done
```

---

## Troubleshooting

### Build Error: "credentials.h: No such file"

**Solution:** Copy the example file:
```bash
cp credentials.h.example credentials.h
# Then edit credentials.h with your values
```

### Credentials Not Working

1. Check `credentials.h` has correct values (no quotes around values in defines)
2. Rebuild firmware (credentials are compile-time, not runtime)
3. Verify MQTT broker address in `config.h` matches your HiveMQ cluster
4. Check serial monitor for connection errors

### Want to Change Credentials

1. Edit `credentials.h`
2. Rebuild: `pio run`
3. Reflash ESP32
4. Old credentials are replaced in new firmware

---

## Best Practices

✅ **Do:**
- Keep `credentials.h` in your password manager
- Use different credentials per deployment/environment
- Rotate credentials periodically
- Document which nodes use which credentials

❌ **Don't:**
- Commit `credentials.h` to Git
- Share `.bin` files publicly
- Use the same credentials across all nodes (if one is compromised, all are)
- Leave default/example credentials in production

---

## For Deployment to End Users

Since end users only configure WiFi (not MQTT):

1. **Build firmware** with your organization's MQTT credentials
2. **Distribute** the `.bin` file to end users
3. **End users** flash via ESP Web Tools
4. **End users** configure only WiFi (via captive portal)
5. **All nodes** connect to your MQTT broker automatically

This centralizes credential management while allowing distributed WiFi setup.

---

**Questions?** See [README.md](README.md) for full documentation.
