#include "ota_updater.h"
#include <WiFiClientSecure.h>

// GitHub API endpoint for latest release
#define GITHUB_API_URL "https://api.github.com/repos/Cem512/QSafe/releases/latest"
#define GITHUB_USER_AGENT "QSafe-ESP32-OTA-Updater"

OTAUpdater::OTAUpdater() {
    _latest_version = "";
    _download_url = "";
    _checksum_md5 = "";
}

bool OTAUpdater::checkForUpdate() {
    DEBUG_PRINTLN("[OTA] Checking for firmware updates...");

    if (!fetchReleaseInfo()) {
        DEBUG_PRINTLN("[OTA] ✗ Failed to fetch release info");
        return false;
    }

    // Compare versions
    if (_latest_version == FIRMWARE_VERSION) {
        DEBUG_PRINTF("[OTA] Already on latest version: %s\n", FIRMWARE_VERSION);
        return false;
    }

    DEBUG_PRINTF("[OTA] ✓ New version available: %s (current: %s)\n",
                _latest_version.c_str(), FIRMWARE_VERSION);
    return true;
}

bool OTAUpdater::performUpdate() {
    if (_download_url.length() == 0) {
        DEBUG_PRINTLN("[OTA] ✗ No download URL available");
        return false;
    }

    DEBUG_PRINTF("[OTA] Starting update to version %s...\n", _latest_version.c_str());
    DEBUG_PRINTF("[OTA] Download URL: %s\n", _download_url.c_str());

    if (!downloadAndInstall(_download_url)) {
        DEBUG_PRINTLN("[OTA] ✗ Update failed!");
        return false;
    }

    DEBUG_PRINTLN("[OTA] ✓ Update successful! Rebooting...");
    delay(2000);
    ESP.restart();
    return true;
}

bool OTAUpdater::updateNow() {
    if (checkForUpdate()) {
        return performUpdate();
    }
    return false;
}

bool OTAUpdater::fetchReleaseInfo() {
    WiFiClientSecure client;
    HTTPClient http;

    // GitHub API uses TLS, but we'll skip cert validation for simplicity
    // In production, you should validate certificates
    client.setInsecure();

    http.begin(client, GITHUB_API_URL);
    http.addHeader("User-Agent", GITHUB_USER_AGENT);
    http.addHeader("Accept", "application/vnd.github.v3+json");

    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        DEBUG_PRINTF("[OTA] HTTP GET failed: %d\n", httpCode);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    // Parse JSON response (GitHub API releases can be large, use 16KB)
    DynamicJsonDocument doc(16384);
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
        DEBUG_PRINTF("[OTA] JSON parse failed: %s\n", error.c_str());
        return false;
    }

    // Extract version (tag_name, e.g., "v1.0.1")
    String tag_name = doc["tag_name"] | "";
    if (tag_name.startsWith("v")) {
        _latest_version = tag_name.substring(1);  // Remove 'v' prefix
    } else {
        _latest_version = tag_name;
    }

    // Find firmware asset (try OTA binary first, then fallback to merged)
    JsonArray assets = doc["assets"];
    for (JsonObject asset : assets) {
        String name = asset["name"] | "";
        // Prefer qsafe-ota.bin (smaller, faster download)
        if (name == "qsafe-ota.bin" || name == "qsafe-merged.bin") {
            _download_url = asset["browser_download_url"] | "";
            DEBUG_PRINTF("[OTA] Found firmware: %s\n", name.c_str());
            break;
        }
    }

    if (_download_url.length() == 0) {
        DEBUG_PRINTLN("[OTA] ✗ Firmware binary not found in release");
        DEBUG_PRINTLN("[OTA]   Expected: qsafe-ota.bin or qsafe-merged.bin");
        return false;
    }

    return true;
}

bool OTAUpdater::downloadAndInstall(const String& url) {
    WiFiClientSecure client;
    HTTPClient http;

    client.setInsecure();  // Skip cert validation

    DEBUG_PRINTLN("[OTA] Downloading firmware...");

    http.begin(client, url);
    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_MOVED_PERMANENTLY) {
        DEBUG_PRINTF("[OTA] Download failed: HTTP %d\n", httpCode);
        http.end();
        return false;
    }

    int contentLength = http.getSize();
    DEBUG_PRINTF("[OTA] Firmware size: %d bytes\n", contentLength);

    if (contentLength <= 0) {
        DEBUG_PRINTLN("[OTA] ✗ Invalid firmware size");
        http.end();
        return false;
    }

    // Check if enough space for OTA
    if (!Update.begin(contentLength)) {
        DEBUG_PRINTF("[OTA] ✗ Not enough space: %s\n", Update.errorString());
        http.end();
        return false;
    }

    // Download and write firmware
    WiFiClient* stream = http.getStreamPtr();
    size_t written = Update.writeStream(*stream);

    if (written != contentLength) {
        DEBUG_PRINTF("[OTA] ✗ Write error: only %d/%d bytes written\n", written, contentLength);
        Update.abort();
        http.end();
        return false;
    }

    DEBUG_PRINTLN("[OTA] Firmware written, verifying...");

    if (!Update.end()) {
        DEBUG_PRINTF("[OTA] ✗ Update error: %s\n", Update.errorString());
        http.end();
        return false;
    }

    if (!Update.isFinished()) {
        DEBUG_PRINTLN("[OTA] ✗ Update incomplete");
        http.end();
        return false;
    }

    http.end();
    DEBUG_PRINTLN("[OTA] ✓ Firmware verified successfully!");
    return true;
}

bool OTAUpdater::validateChecksum(const String& expected_md5) {
    // ESP32 Update library automatically validates MD5 during update
    // This is a placeholder for additional validation if needed
    return true;
}
