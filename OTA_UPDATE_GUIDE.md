# QSafe OTA Update Guide

## Overview

QSafe nodes support **Over-The-Air (OTA)** firmware updates directly from GitHub Releases. Updates can be triggered automatically (daily) or manually via MQTT commands.

## How It Works

1. **Daily Check:** Node checks GitHub for new releases every 24 hours
2. **Download:** If available, downloads `qsafe-merged.bin` from latest release
3. **Install:** Writes firmware to OTA partition
4. **Reboot:** Automatically reboots to new firmware
5. **Rollback:** If new firmware fails to boot, ESP32 automatically rolls back to previous version

## Creating a Release

### 1. Build the Firmware

```bash
cd C:\Users\cemfi\Documents\PlatformIO\Projects\QSafe
pio run
```

This creates two firmware files:
- **`.pio\build\esp32dev\qsafe-ota.bin`** - OTA updates (~800 KB)
- **`.pio\build\esp32dev\qsafe-merged.bin`** - Full flash image (~2-4 MB)

### 2. Create GitHub Release

1. Go to: https://github.com/Cem512/QSafe/releases/new
2. **Tag version:** `v1.0.1` (increment from current version)
3. **Release title:** `QSafe v1.0.1 - Description`
4. **Description:** Changelog (what's new/fixed)
5. **Attach files:**
   - Upload **`qsafe-ota.bin`** (recommended - smaller, faster OTA)
   - Optionally upload `qsafe-merged.bin` (for initial flash)
6. Click **"Publish release"**

### 3. Nodes Auto-Update

- Nodes check daily for updates
- If `v1.0.1` > current version → auto-install
- Sends MQTT notification before/after update
- Reboots automatically

## Manual Update via MQTT

Send MQTT command to specific node:

**Topic:** `eew/cmd/Node_XXXX`

**Payload:**
```json
{
  "command": "update"
}
```

**Response (on `eew/status/Node_XXXX`):**
```json
{
  "command": "update",
  "status": "updating",
  "current_version": "1.0.0",
  "new_version": "1.0.1"
}
```

## Version Management

### Update Firmware Version

Edit `src/config.h`:

```cpp
#define FIRMWARE_VERSION "1.0.1"  // Increment this!
```

### Version Format

Use semantic versioning: `MAJOR.MINOR.PATCH`
- **MAJOR:** Breaking changes
- **MINOR:** New features (backwards compatible)
- **PATCH:** Bug fixes

**Examples:**
- `1.0.0` → `1.0.1` (bug fix)
- `1.0.1` → `1.1.0` (new feature: S-wave detection)
- `1.1.0` → `2.0.0` (breaking change: new MQTT format)

## Rollback Protection

ESP32 has **dual OTA partitions**:

1. **Boot partition 0:** Current firmware
2. **Boot partition 1:** Previous firmware (backup)

If new firmware fails to boot (crashes, won't start), ESP32 automatically boots from the backup partition.

### Manual Rollback

If you need to force rollback, send:

```json
{
  "command": "restart"
}
```

Then reflash previous firmware version.

## Testing Updates

### Test Procedure

1. **Local test first:**
   - Build and upload via USB
   - Verify node works correctly
   - Monitor serial output for 5 minutes

2. **Create pre-release:**
   - GitHub → New Release → Check "Pre-release"
   - Test OTA update on ONE node only

3. **Production release:**
   - If test node works for 24 hours → publish full release
   - All nodes will auto-update within 24 hours

### Staged Rollout (Advanced)

For large deployments, you can:
1. Create release but don't publish
2. Manually trigger update on 10% of nodes
3. Monitor for 24 hours
4. If stable → publish release for all nodes

## Monitoring Updates

### MQTT Status Messages

**Before Update:**
```json
{
  "type": "ota_update_start",
  "current_version": "1.0.0",
  "new_version": "1.0.1"
}
```

**After Successful Update:**
Node reboots, sends normal heartbeat with new version:
```json
{
  "type": "heartbeat",
  "firmware_version": "1.0.1",
  ...
}
```

### Serial Monitor

```
[OTA] Performing daily update check...
[OTA] ✓ New version available: 1.0.1 (current: 1.0.0)
[OTA] Starting update to version 1.0.1...
[OTA] Downloading firmware...
[OTA] Firmware size: 1245678 bytes
[OTA] Firmware written, verifying...
[OTA] ✓ Firmware verified successfully!
[OTA] ✓ Update successful! Rebooting...
```

## Troubleshooting

### "No download URL available"
- Ensure `qsafe-merged.bin` is attached to GitHub release
- File must be named exactly `qsafe-merged.bin`

### "Not enough space"
- ESP32 needs ~1.5MB free for OTA
- Check partition scheme in `platformio.ini`
- Current scheme: `min_spiffs.csv` (OTA-enabled)

### "Download failed: HTTP 404"
- Release might be draft/private
- Ensure release is published
- Check repository is public or use authentication

### Update never triggers
- Check WiFi connection: `WiFi.status() == WL_CONNECTED`
- Check daily interval: Updates check every 24 hours
- Force check: Send MQTT command `{"command": "update"}`
- Check serial monitor for errors

### Node won't boot after update
- ESP32 auto-rollback activates
- Check serial monitor for crash logs
- Reflash via USB if needed

## Security Considerations

### Current Implementation
- ✅ HTTPS download from GitHub
- ✅ Version validation
- ✅ Size validation
- ✅ Automatic rollback on failure
- ⚠️ Certificate validation disabled (for simplicity)

### Production Recommendations
1. **Enable TLS certificate validation** (currently `client.setInsecure()`)
2. **Add MD5 checksum validation** (GitHub releases support SHA256)
3. **Sign firmware** with cryptographic signature
4. **Rate limiting** (avoid too-frequent checks)

### Example: Add Checksum Validation

In GitHub release description, add:
```
MD5: a1b2c3d4e5f6...
```

Code will validate before installing.

## Update Workflow Example

### Scenario: Bug Fix Release

1. **Fix bug** in code
2. **Update version:**
   ```cpp
   #define FIRMWARE_VERSION "1.0.1"  // was 1.0.0
   ```
3. **Build:**
   ```bash
   pio run
   ```
4. **Test locally via USB**
5. **Create release on GitHub:**
   - Tag: `v1.0.1`
   - Title: "QSafe v1.0.1 - Fix WiFi reconnection bug"
   - Upload: `qsafe-merged.bin`
6. **Publish release**
7. **Monitor nodes:**
   - Within 24 hours, all nodes auto-update
   - Check MQTT for update status messages
   - Verify heartbeats show new version

## Best Practices

✅ **DO:**
- Test updates locally first
- Use semantic versioning
- Write detailed release notes
- Monitor node status after updates
- Keep previous firmware binary for rollback

❌ **DON'T:**
- Skip local testing
- Deploy untested updates to all nodes
- Change MQTT protocol without backward compatibility
- Update during seismic events (nodes may miss P-wave)

## FAQ

**Q: How long does an update take?**
A: ~30-60 seconds (download + install + reboot)

**Q: Will nodes miss earthquakes during update?**
A: Yes, during 30-60s update window. Schedule updates during low-seismic activity.

**Q: Can I force update immediately?**
A: Yes, send MQTT command `{"command": "update"}`

**Q: What if GitHub is down?**
A: Update check fails gracefully. Node continues normal operation. Retries next day.

**Q: Can I rollback to older version?**
A: Publish older version as new release (e.g., `v1.0.0-rollback`). Nodes will "update" to it.

**Q: How do I disable auto-updates?**
A: Comment out OTA check in `main.cpp` loop, or set `OTA_CHECK_INTERVAL_MS` to very large value.

---

**Need help?** Open an issue at: https://github.com/Cem512/QSafe/issues
