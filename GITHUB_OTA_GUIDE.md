# GitHub Automatic OTA Updates - Setup Guide

This guide explains how to set up and use the automatic GitHub-based OTA (Over-The-Air) firmware update system for your ESP32 seismograph nodes.

## Table of Contents

1. [Overview](#overview)
2. [How It Works](#how-it-works)
3. [Initial Setup](#initial-setup)
4. [Creating a Release](#creating-a-release)
5. [Configuration](#configuration)
6. [Monitoring Updates](#monitoring-updates)
7. [Troubleshooting](#troubleshooting)

---

## Overview

The ESP32 seismograph nodes now have **automatic firmware update capability** that checks GitHub for new releases and updates themselves without manual intervention.

### Key Features

- ✅ **Automatic version checking** every 24 hours
- ✅ **Semantic versioning** support (e.g., v1.2.3)
- ✅ **MD5 verification** for firmware integrity
- ✅ **Automated builds** via GitHub Actions
- ✅ **Zero-touch updates** for deployed nodes
- ✅ **Rollback protection** (only updates to newer versions)

---

## How It Works

### Build & Release Workflow

```
┌─────────────────────────────────────────────────────────────┐
│  Developer pushes code with version tag (e.g., v1.2.3)      │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│  GitHub Actions automatically:                              │
│  1. Builds firmware binary                                  │
│  2. Generates MD5 checksum                                  │
│  3. Creates firmware metadata JSON                          │
│  4. Publishes GitHub Release with assets                    │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│  ESP32 nodes (every 24 hours):                              │
│  1. Query GitHub API for latest release                     │
│  2. Compare current version vs latest version               │
│  3. If newer version available, download .bin file          │
│  4. Verify MD5 checksum                                     │
│  5. Flash firmware and reboot                               │
└─────────────────────────────────────────────────────────────┘
```

### Update Check Frequency

- **Default:** Every 24 hours
- **On Boot:** First check happens within 1 minute of boot
- **Configurable:** Edit `OTA_CHECK_INTERVAL_MS` in [version.h](esp32-node/include/version.h)

---

## Initial Setup

### 1. Configure GitHub Repository Information

Edit [esp32-node/include/version.h](esp32-node/include/version.h):

```cpp
#define GITHUB_REPO_OWNER "YOUR_GITHUB_USERNAME"    // Change this!
#define GITHUB_REPO_NAME "seismograph-system"       // Change if different!
```

**Example:**
```cpp
#define GITHUB_REPO_OWNER "johndoe"
#define GITHUB_REPO_NAME "seismograph-system"
```

### 2. Push to GitHub

```bash
git add .
git commit -m "Configure GitHub OTA updates"
git push origin main
```

### 3. Verify GitHub Actions

- Go to your repository on GitHub
- Click the **Actions** tab
- You should see the "Build ESP32 Firmware" workflow
- On every push to `main`, it will build the firmware

---

## Creating a Release

### Method 1: Using Git Tags (Recommended)

1. **Commit your changes:**
   ```bash
   git add .
   git commit -m "Add new feature"
   git push
   ```

2. **Create and push a version tag:**
   ```bash
   git tag v1.2.3
   git push origin v1.2.3
   ```

3. **GitHub Actions will automatically:**
   - Build the firmware
   - Create a GitHub Release
   - Upload firmware binary and metadata
   - ESP32 nodes will detect and install the update within 24 hours

### Method 2: Manual Release (GitHub UI)

1. Go to your repository on GitHub
2. Click **Releases** → **Draft a new release**
3. Click **Choose a tag** → Type new tag (e.g., `v1.2.4`) → **Create new tag**
4. Enter release title and description
5. Click **Publish release**
6. GitHub Actions will build and attach the firmware automatically

---

## Configuration

### Version Header File

Located at: [esp32-node/include/version.h](esp32-node/include/version.h)

```cpp
#define FIRMWARE_VERSION "1.0.0-local"           // Current version (auto-updated by CI)
#define BUILD_TIMESTAMP "manual-build"            // Build timestamp
#define GIT_COMMIT "unknown"                      // Git commit hash

#define GITHUB_REPO_OWNER "YOUR_GITHUB_USERNAME"  // GitHub username
#define GITHUB_REPO_NAME "seismograph-system"     // Repository name

#define OTA_CHECK_INTERVAL_MS (24 * 60 * 60 * 1000)  // Check interval (24 hours)
#define GITHUB_API_TIMEOUT_MS 10000                   // API timeout (10 seconds)
```

### Disabling Auto-Update

In [esp32-node/src/main.cpp](esp32-node/src/main.cpp), set:

```cpp
bool autoUpdateEnabled = false;  // Disable automatic updates
```

Or add a web interface toggle in the configuration portal.

---

## Monitoring Updates

### Serial Monitor Output

Connect to your ESP32 via serial monitor (115200 baud) to see OTA activity:

```
[GitHub OTA] Checking for firmware updates...
[GitHub OTA] Current version: 1.0.0
[GitHub OTA] API URL: https://api.github.com/repos/user/repo/releases/latest
[GitHub OTA] Latest version: 1.2.3
[GitHub OTA] New version available!
[GitHub OTA] Found firmware: seismograph-esp32-1.2.3.bin
[GitHub OTA] MD5: a3d5f8e9c1b2a4d6e7f8a9b0c1d2e3f4
[GitHub OTA] Starting download and update...
[GitHub OTA] URL: https://github.com/.../seismograph-esp32-1.2.3.bin
[GitHub OTA] Firmware size: 1145088 bytes
[GitHub OTA] MD5 verification enabled
[GitHub OTA] Update successful! Rebooting...
```

### No Update Available:

```
[GitHub OTA] Checking for firmware updates...
[GitHub OTA] Current version: 1.2.3
[GitHub OTA] Latest version: 1.2.3
[GitHub OTA] Already running latest version
```

### Error Scenarios:

```
[GitHub OTA] API request failed, HTTP code: 404
```
→ Check that GITHUB_REPO_OWNER and GITHUB_REPO_NAME are correct

```
[GitHub OTA] No firmware binary found in release
```
→ Ensure the release has a `.bin` file attached

```
[GitHub OTA] Download failed, HTTP code: 403
```
→ GitHub API rate limit exceeded (60 requests/hour for unauthenticated)

---

## Troubleshooting

### Problem: ESP32 doesn't update

**Possible causes:**

1. **Wrong repository configuration**
   - Verify `GITHUB_REPO_OWNER` and `GITHUB_REPO_NAME` in [version.h](esp32-node/include/version.h)

2. **No internet connection**
   - ESP32 must be in Station Mode (connected to WiFi)
   - Check WiFi credentials in configuration portal

3. **Auto-update disabled**
   - Check `autoUpdateEnabled` variable in [main.cpp](esp32-node/src/main.cpp)

4. **Version comparison issue**
   - Ensure releases use semantic versioning: `v1.2.3`
   - Dev builds (e.g., `dev-abc123`) won't auto-update

5. **GitHub API rate limit**
   - Unauthenticated API calls: 60/hour
   - Wait 1 hour or add GitHub token (advanced)

### Problem: Build fails in GitHub Actions

1. **Check Actions log**
   - Go to repository → Actions → Click failed workflow
   - Review error messages

2. **Common issues:**
   - Missing dependencies in `platformio.ini`
   - Syntax errors in code
   - PlatformIO version mismatch

3. **Test locally:**
   ```bash
   cd esp32-node
   pio run
   ```

### Problem: Update downloads but fails to flash

**Possible causes:**

1. **Insufficient flash space**
   - Firmware exceeds available OTA partition
   - Check partition table in `platformio.ini`

2. **MD5 mismatch**
   - Corrupted download
   - Network issue during download
   - ESP32 will retry on next check

3. **Power failure during update**
   - ESP32 has rollback protection
   - Previous firmware remains intact
   - Will retry on next boot

### Forcing an Immediate Update Check

**Method 1: Restart ESP32**
```cpp
ESP.restart();
```

**Method 2: Serial command** (if implemented)
```
check_update
```

**Method 3: Temporary interval override**

Edit [version.h](esp32-node/include/version.h):
```cpp
#define OTA_CHECK_INTERVAL_MS (60 * 1000)  // Check every 60 seconds (for testing)
```

Remember to change it back after testing!

---

## Version Numbering Best Practices

### Semantic Versioning (SemVer)

Use the format: `v{MAJOR}.{MINOR}.{PATCH}`

- **MAJOR:** Breaking changes (e.g., v2.0.0)
- **MINOR:** New features, backward compatible (e.g., v1.3.0)
- **PATCH:** Bug fixes (e.g., v1.2.5)

### Examples:

✅ **Good:**
- `v1.0.0` - Initial release
- `v1.1.0` - Added MQTT streaming feature
- `v1.1.1` - Fixed WiFi reconnection bug
- `v2.0.0` - Changed API protocol (breaking)

❌ **Bad:**
- `release-2024-01-15` - Not parseable
- `v1.2` - Missing patch number
- `1.2.3` - Missing 'v' prefix (works but inconsistent)

---

## Security Considerations

### Current Implementation:

- ✅ **HTTPS downloads** from GitHub
- ✅ **MD5 verification** of firmware binaries
- ⚠️ **Certificate validation disabled** (`setInsecure()` for simplicity)
- ⚠️ **No firmware signing** (digital signatures)

### Recommendations for Production:

1. **Enable certificate validation**
   - Add GitHub's root CA certificate
   - Remove `setInsecure()` calls

2. **Implement firmware signing**
   - Sign binaries with private key
   - Verify signature before flashing

3. **Rate limiting**
   - Add GitHub Personal Access Token for higher API limits
   - Implement exponential backoff on failures

4. **Update windows**
   - Schedule updates during low-activity periods
   - Avoid updates during seismic events

---

## Advanced: Manual OTA via MQTT (Development)

For local testing, you can still use the MQTT OTA method:

```python
python trigger_ota_update.py
```

This bypasses GitHub and uses a local HTTP server. See [MQTT_OTA_GUIDE.md](MQTT_OTA_GUIDE.md) for details.

---

## Files Modified for GitHub OTA

1. ✅ [.github/workflows/build-firmware.yml](.github/workflows/build-firmware.yml) - CI/CD pipeline
2. ✅ [esp32-node/include/version.h](esp32-node/include/version.h) - Version configuration
3. ✅ [esp32-node/src/main.cpp](esp32-node/src/main.cpp) - OTA logic
4. ✅ [esp32-node/platformio.ini](esp32-node/platformio.ini) - Build flags
5. ✅ [GITHUB_OTA_GUIDE.md](GITHUB_OTA_GUIDE.md) - This document
6. ✅ [README.md](README.md) - Updated with OTA information

---

## Next Steps

1. **Configure repository details** in [version.h](esp32-node/include/version.h)
2. **Push to GitHub** and verify Actions workflow runs
3. **Create your first release** (`git tag v1.0.0 && git push origin v1.0.0`)
4. **Monitor serial output** on your ESP32 nodes
5. **Test the update** by creating a v1.0.1 release

---

## Support

- **GitHub Issues:** Report bugs at your repository's Issues page
- **Documentation:** See [README.md](README.md) for general project info
- **MQTT OTA:** See [MQTT_OTA_GUIDE.md](MQTT_OTA_GUIDE.md) for local development updates

---

**Last Updated:** 2024-11-22
**Version:** 1.0.0
