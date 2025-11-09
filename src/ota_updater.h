#ifndef OTA_UPDATER_H
#define OTA_UPDATER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "config.h"

class OTAUpdater {
public:
    OTAUpdater();

    // Check for new firmware from GitHub releases
    bool checkForUpdate();

    // Perform OTA update
    bool performUpdate();

    // Get current firmware version
    String getCurrentVersion() { return FIRMWARE_VERSION; }

    // Get latest available version
    String getLatestVersion() { return _latest_version; }

    // Manual update trigger
    bool updateNow();

private:
    String _latest_version;
    String _download_url;
    String _checksum_md5;

    bool fetchReleaseInfo();
    bool downloadAndInstall(const String& url);
    bool validateChecksum(const String& expected_md5);
};

#endif // OTA_UPDATER_H
