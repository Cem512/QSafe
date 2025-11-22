# Quick Start: GitHub OTA Setup

## 5-Minute Setup Guide

### Step 1: Configure Your Repository Info
Edit `esp32-node/include/version.h`:

```cpp
#define GITHUB_REPO_OWNER "YOUR_GITHUB_USERNAME"  // ← Change this!
#define GITHUB_REPO_NAME "seismograph-system"     // ← Change if different!
```

### Step 2: Push to GitHub

```bash
git add .
git commit -m "Configure GitHub OTA updates"
git push origin main
```

### Step 3: Create Your First Release

```bash
git tag v1.0.0
git push origin v1.0.0
```

### Step 4: Verify

1. Go to your GitHub repository → **Actions** tab
2. You should see "Build ESP32 Firmware" workflow running
3. After completion, go to **Releases** tab
4. You should see `v1.0.0` release with firmware binary

### Step 5: Flash ESP32 (First Time)

```bash
cd esp32-node
pio run --target upload
```

### Done! 🎉

Your ESP32 nodes will now:
- Check for updates every 24 hours
- Automatically download and install new firmware
- Reboot with the new version

## Creating Future Updates

Just create a new tag:

```bash
git add .
git commit -m "Add new feature"
git push

git tag v1.1.0
git push origin v1.1.0
```

GitHub Actions builds it, ESP32s update themselves within 24 hours!

## Monitoring Updates

Connect via serial monitor (115200 baud):

```
[GitHub OTA] Checking for firmware updates...
[GitHub OTA] Current version: 1.0.0
[GitHub OTA] Latest version: 1.1.0
[GitHub OTA] New version available!
[GitHub OTA] Starting download and update...
[GitHub OTA] Update successful! Rebooting...
```

## Troubleshooting

**ESP32 not updating?**
1. Check WiFi connection (must be in Station mode, not AP mode)
2. Verify `GITHUB_REPO_OWNER` and `GITHUB_REPO_NAME` in version.h
3. Check serial monitor for error messages

**Build failing?**
1. Go to GitHub → Actions → Click failed workflow
2. Review error log
3. Test locally: `cd esp32-node && pio run`

## Full Documentation

For complete details, see [GITHUB_OTA_GUIDE.md](GITHUB_OTA_GUIDE.md)
