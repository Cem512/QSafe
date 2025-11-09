#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

#include "config.h"
#include "adxl345.h"
#include "calibration.h"
#include "trigger.h"
#include "mqtt_handler.h"
#include "ota_updater.h"

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================

ADXL345 accel;  // I2C mode - no pin argument needed
Calibration calibration;
TriggerDetector trigger_detector;
MQTTHandler mqtt;
OTAUpdater ota;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_SERVER_DEFAULT, TIMEZONE_OFFSET_SEC, NTP_UPDATE_INTERVAL);

Preferences prefs;

// ============================================================================
// STATE VARIABLES
// ============================================================================

String node_id;
String hardware_id;
bool system_ready = false;
bool is_calibrated = false;

// Circular buffer for waveform storage
sample_t waveform_buffer[BUFFER_SIZE_SAMPLES];
int buffer_index = 0;
bool buffer_full = false;

// Timing
unsigned long last_heartbeat = 0;
unsigned long last_calibration_check = 0;
unsigned long trigger_time = 0;
bool in_cooldown = false;

// WiFi resilience
unsigned long last_wifi_check = 0;
unsigned long wifi_reconnect_count = 0;
#define WIFI_CHECK_INTERVAL_MS 10000  // Check every 10 seconds

// OTA update check
unsigned long last_ota_check = 0;
#define OTA_CHECK_INTERVAL_MS 86400000UL  // 24 hours (daily check)

// Sequence number for MQTT messages
uint32_t sequence_number = 0;

// Statistics
uint32_t triggers_24h = 0;
unsigned long triggers_24h_reset = 0;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

void setupWiFi();
void setupNodeIdentity();
void performInitialCalibration();
void samplingTask(void* parameter);
void mqttCallback(char* topic, byte* payload, unsigned int length);
void sendHeartbeat();
void sendTriggerData(TriggerInfo& info);
String compressWaveform();
void handleCommand(JsonDocument& cmd);

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(1000);
    
    DEBUG_PRINTLN("\n\n");
    DEBUG_PRINTLN("╔══════════════════════════════════════════╗");
    DEBUG_PRINTLN("║   EEW Node Firmware v" FIRMWARE_VERSION "             ║");
    DEBUG_PRINTLN("║   Earthquake Early Warning System        ║");
    DEBUG_PRINTLN("╚══════════════════════════════════════════╝");
    DEBUG_PRINTLN("");
    
    // Initialize LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    
    // Step 1: Setup node identity
    DEBUG_PRINTLN("[SETUP] Getting node identity...");
    setupNodeIdentity();

    // Step 2: Setup WiFi
    DEBUG_PRINTLN("[SETUP] Connecting to WiFi...");
    setupWiFi();
    
    // Step 3: Initialize NTP
    DEBUG_PRINTLN("[SETUP] Syncing time...");
    timeClient.begin();
    timeClient.update();
    DEBUG_PRINTF("[NTP] Current time: %s\n", timeClient.getFormattedTime().c_str());
    
    // Step 4: Initialize ADXL345
    DEBUG_PRINTLN("[SETUP] Initializing ADXL345...");
    if (!accel.begin()) {
        DEBUG_PRINTLN("[SETUP] ✗ ADXL345 initialization failed!");
        DEBUG_PRINTLN("[SETUP] Check wiring and restart.");
        while (1) {
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
            delay(200);  // Fast blink = error
        }
    }
    
    // Step 5: Load or perform calibration
    DEBUG_PRINTLN("[SETUP] Loading calibration...");
    if (calibration.loadFromNVS()) {
        DEBUG_PRINTLN("[SETUP] ✓ Using saved calibration");
        // Apply the loaded scale factor to the accelerometer
        accel.setScaleFactor(calibration.getScaleFactor());
        is_calibrated = true;
    } else {
        DEBUG_PRINTLN("[SETUP] No calibration found - performing now...");
        performInitialCalibration();
    }
    
    // Step 6: Initialize MQTT
    DEBUG_PRINTLN("[SETUP] Connecting to MQTT...");
    if (!mqtt.begin(node_id.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
        DEBUG_PRINTLN("[SETUP] ✗ MQTT initialization failed!");
        DEBUG_PRINTLN("[SETUP] Check MQTT credentials in credentials.h");
        while (1) {
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
            delay(500);  // Slow blink = MQTT error
        }
    }
    mqtt.setCallback(mqttCallback);
    
    // Step 7: Register with server
    DEBUG_PRINTLN("[SETUP] Registering with server...");
    mqtt.publishRegistration(node_id.c_str(), hardware_id.c_str(), FIRMWARE_VERSION);
    
    // Step 8: Start sampling task
    DEBUG_PRINTLN("[SETUP] Starting sampling task...");
    xTaskCreatePinnedToCore(
        samplingTask,
        "Sampling",
        STACK_SIZE,
        NULL,
        1,  // Priority
        NULL,
        1   // Core 1 (Core 0 for WiFi/MQTT)
    );
    
    system_ready = true;
    DEBUG_PRINTLN("\n[SETUP] ✓✓✓ SYSTEM READY ✓✓✓\n");
    
    // LED: solid on = ready
    digitalWrite(LED_PIN, HIGH);
}

// ============================================================================
// MAIN LOOP (Core 0 - handles WiFi/MQTT)
// ============================================================================

void loop() {
    unsigned long now = millis();

    // Monitor WiFi connection and auto-reconnect if needed
    if (now - last_wifi_check > WIFI_CHECK_INTERVAL_MS) {
        if (WiFi.status() != WL_CONNECTED) {
            DEBUG_PRINTLN("[WiFi] Connection lost! Attempting to reconnect...");
            wifi_reconnect_count++;

            WiFi.reconnect();
            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 20) {  // 10 seconds max
                delay(500);
                DEBUG_PRINT(".");
                attempts++;
            }

            if (WiFi.status() == WL_CONNECTED) {
                DEBUG_PRINTLN("\n[WiFi] ✓ Reconnected!");
                DEBUG_PRINTF("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
                DEBUG_PRINTF("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());
                DEBUG_PRINTF("[WiFi] Reconnect count: %lu\n", wifi_reconnect_count);

                // Reconnect MQTT after WiFi is restored
                mqtt.loop();  // This will trigger auto-reconnect in MQTT handler
            } else {
                DEBUG_PRINTLN("\n[WiFi] ✗ Reconnection failed. Will retry in 10s...");
            }
        }
        last_wifi_check = now;
    }

    // Update MQTT (will auto-reconnect if WiFi is available)
    mqtt.loop();

    // Update NTP periodically (skip if no WiFi)
    if (WiFi.status() == WL_CONNECTED) {
        timeClient.update();
    }

    // Send heartbeat
    if (now - last_heartbeat > HEARTBEAT_INTERVAL_MS) {
        sendHeartbeat();
        last_heartbeat = now;
    }
    
    // Check for OTA updates (daily, only if WiFi connected)
    if (WiFi.status() == WL_CONNECTED && now - last_ota_check > OTA_CHECK_INTERVAL_MS) {
        DEBUG_PRINTLN("[OTA] Performing daily update check...");
        if (ota.checkForUpdate()) {
            DEBUG_PRINTF("[OTA] New version %s available! Auto-installing...\n", ota.getLatestVersion().c_str());

            // Notify server before update
            StaticJsonDocument<256> doc;
            doc["type"] = "ota_update_start";
            doc["current_version"] = FIRMWARE_VERSION;
            doc["new_version"] = ota.getLatestVersion();
            mqtt.publishStatus(node_id.c_str(), doc);

            delay(1000);  // Give MQTT time to send

            ota.performUpdate();  // Will restart on success
        }
        last_ota_check = now;
    }

    // Check calibration drift (hourly)
    if (now - last_calibration_check > CALIBRATION_CHECK_HRS * 3600000UL) {
        float ax, ay, az;
        accel.readAcceleration(ax, ay, az);
        float gravity_mag = sqrt(ax*ax + ay*ay + az*az);
        
        if (calibration.checkDrift(gravity_mag)) {
            DEBUG_PRINTLN("[DRIFT] Recalibration recommended");
            // Send status alert to server
            StaticJsonDocument<256> doc;
            doc["type"] = "calibration_drift";
            doc["gravity_current"] = gravity_mag;
            doc["gravity_calibrated"] = calibration.getData().gravity_mag;
            mqtt.publishStatus(node_id.c_str(), doc);
        }
        
        last_calibration_check = now;
    }
    
    // Reset 24h trigger counter
    if (now - triggers_24h_reset > 86400000UL) {  // 24 hours
        triggers_24h = 0;
        triggers_24h_reset = now;
    }
    
    // Check heap memory
    if (ESP.getFreeHeap() < MIN_FREE_HEAP_KB * 1024) {
        DEBUG_PRINTF("[WARN] Low heap: %d bytes\n", ESP.getFreeHeap());
    }
    
    delay(100);
}

// ============================================================================
// SAMPLING TASK (Core 1 - dedicated to sensor reading)
// ============================================================================

void samplingTask(void* parameter) {
    DEBUG_PRINTLN("[SAMPLING] Task started on Core 1");
    
    const TickType_t sample_interval = pdMS_TO_TICKS(1000 / SAMPLE_RATE_HZ);
    TickType_t last_wake_time = xTaskGetTickCount();
    
    while (true) {
        // Precise timing using FreeRTOS delay
        vTaskDelayUntil(&last_wake_time, sample_interval);
        
        // Read accelerometer
        int16_t raw_x, raw_y, raw_z;
        accel.readRaw(raw_x, raw_y, raw_z);
        
        // Convert to mg
        float ax_mg = raw_x * ADXL_SCALE_FACTOR;
        float ay_mg = raw_y * ADXL_SCALE_FACTOR;
        float az_mg = raw_z * ADXL_SCALE_FACTOR;
        
        // Apply calibration
        float corrected_x, corrected_y, corrected_z;
        calibration.applyRotation(
            MG_TO_G(ax_mg), MG_TO_G(ay_mg), MG_TO_G(az_mg),
            corrected_x, corrected_y, corrected_z
        );
        
        // Store Z-axis in buffer (convert back to mg for trigger detection)
        float z_mg = corrected_z * 1000.0;
        
        // Store as int16 for waveform transmission
        waveform_buffer[buffer_index] = (sample_t)(corrected_z * 1000.0 / ADXL_SCALE_FACTOR);
        
        buffer_index++;
        if (buffer_index >= BUFFER_SIZE_SAMPLES) {
            buffer_index = 0;
            buffer_full = true;
        }
        
        // Check cooldown
        if (in_cooldown) {
            if (millis() - trigger_time > COOLDOWN_TIME_MS) {
                in_cooldown = false;
                trigger_detector.reset();
                DEBUG_PRINTLN("[TRIGGER] Cooldown complete, monitoring resumed");
            }
            continue;  // Skip trigger detection during cooldown
        }
        
        // Add sample to trigger detector
        trigger_detector.addSample(z_mg);
        
        // Check for trigger
        TriggerInfo info;
        if (trigger_detector.checkTrigger(info)) {
            // ✓ Event detected!
            trigger_time = millis();
            in_cooldown = true;
            triggers_24h++;
            
            // Send trigger data to server
            sendTriggerData(info);
            
            // Blink LED
            for (int i = 0; i < 3; i++) {
                digitalWrite(LED_PIN, LOW);
                delay(100);
                digitalWrite(LED_PIN, HIGH);
                delay(100);
            }
        }
    }
}

// ============================================================================
// WIFI SETUP (with WiFiManager for provisioning)
// ============================================================================

void setupWiFi() {
    WiFiManager wm;

    // Set custom AP name using node_id
    String ap_name = String(AP_SSID_PREFIX) + hardware_id.substring(8);  // Last 4 chars

    // Custom portal parameters
    WiFiManagerParameter custom_text("<p><b>Configure WiFi for EEW Node</b></p>");
    wm.addParameter(&custom_text);

    // Timeout for portal
    wm.setConfigPortalTimeout(PROVISION_TIMEOUT_S);

    // Try to connect with saved credentials
    digitalWrite(LED_PIN, HIGH);

    if (!wm.autoConnect(ap_name.c_str(), AP_PASSWORD)) {
        DEBUG_PRINTLN("[WiFi] ✗ Failed to connect, restarting...");
        delay(3000);
        ESP.restart();
    }

    DEBUG_PRINTLN("[WiFi] ✓ Connected!");
    DEBUG_PRINT("[WiFi] IP: ");
    DEBUG_PRINTLN(WiFi.localIP());
    DEBUG_PRINT("[WiFi] RSSI: ");
    DEBUG_PRINT(WiFi.RSSI());
    DEBUG_PRINTLN(" dBm");
}

// ============================================================================
// NODE IDENTITY
// ============================================================================

void setupNodeIdentity() {
    // Get hardware ID from MAC address
    uint64_t chipid = ESP.getEfuseMac();
    hardware_id = String((uint32_t)(chipid >> 32), HEX) + String((uint32_t)chipid, HEX);
    hardware_id.toUpperCase();
    
    // Check if friendly name exists in NVS
    prefs.begin("eew", true);
    String friendly_name = prefs.getString("friendly_name", "");
    prefs.end();
    
    if (friendly_name.length() > 0) {
        node_id = friendly_name;
    } else {
        // Use hardware ID as node ID
        node_id = "Node_" + hardware_id.substring(8);  // Last 4 chars
    }
    
    DEBUG_PRINTF("[ID] Hardware ID: %s\n", hardware_id.c_str());
    DEBUG_PRINTF("[ID] Node ID: %s\n", node_id.c_str());
}

// ============================================================================
// CALIBRATION
// ============================================================================

void performInitialCalibration() {
    DEBUG_PRINTLN("[CAL] Place node on stable surface...");
    DEBUG_PRINTLN("[CAL] Calibration will start in 5 seconds...");

    for (int i = 5; i > 0; i--) {
        DEBUG_PRINTF("[CAL] %d...\n", i);
        delay(1000);
    }

    // Allocate RAW calibration samples on heap to avoid stack overflow
    int16_t* raw_samples_x = (int16_t*)malloc(CALIBRATION_SAMPLES * sizeof(int16_t));
    int16_t* raw_samples_y = (int16_t*)malloc(CALIBRATION_SAMPLES * sizeof(int16_t));
    int16_t* raw_samples_z = (int16_t*)malloc(CALIBRATION_SAMPLES * sizeof(int16_t));

    if (!raw_samples_x || !raw_samples_y || !raw_samples_z) {
        DEBUG_PRINTLN("[CAL] ✗ Memory allocation failed!");
        free(raw_samples_x);
        free(raw_samples_y);
        free(raw_samples_z);
        return;
    }

    DEBUG_PRINTLN("[CAL] Collecting RAW samples for auto-calibration...");

    for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
        // Read RAW sensor values (LSB units) for scale factor calculation
        accel.readRaw(raw_samples_x[i], raw_samples_y[i], raw_samples_z[i]);
        delay(1000 / SAMPLE_RATE_HZ);  // Sample at configured rate

        if (i % 100 == 0) {
            DEBUG_PRINTF("[CAL] Progress: %d/%d\n", i, CALIBRATION_SAMPLES);
        }
    }

    DEBUG_PRINTLN("[CAL] Processing and calculating scale factor...");

    if (calibration.performCalibration(raw_samples_x, raw_samples_y, raw_samples_z, CALIBRATION_SAMPLES)) {
        is_calibrated = true;

        // Apply the calibrated scale factor to the accelerometer
        accel.setScaleFactor(calibration.getScaleFactor());

        DEBUG_PRINTLN("[CAL] ✓ Calibration complete!");
    } else {
        DEBUG_PRINTLN("[CAL] ✗ Calibration failed!");
        DEBUG_PRINTLN("[CAL] System will continue but accuracy may be reduced");
    }

    // Free allocated memory
    free(raw_samples_x);
    free(raw_samples_y);
    free(raw_samples_z);
}

// ============================================================================
// MQTT CALLBACK
// ============================================================================

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    DEBUG_PRINT("[MQTT] Message received on ");
    DEBUG_PRINTLN(topic);
    
    // Parse JSON payload
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    
    if (error) {
        DEBUG_PRINT("[MQTT] JSON parse error: ");
        DEBUG_PRINTLN(error.c_str());
        return;
    }
    
    // Handle alert topic
    if (String(topic) == TOPIC_ALERT) {
        DEBUG_PRINTLN("[ALERT] 🚨 Earthquake alert received!");
        
        float magnitude = doc["magnitude"];
        float eta = doc["s_wave_eta"][node_id];
        
        DEBUG_PRINTF("[ALERT] Magnitude: M%.1f\n", magnitude);
        DEBUG_PRINTF("[ALERT] S-wave ETA: %.1f seconds\n", eta);
        
        // Blink LED rapidly
        for (int i = 0; i < 10; i++) {
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
            delay(50);
        }
        digitalWrite(LED_PIN, HIGH);
        
        return;
    }
    
    // Handle command topic
    String cmd_topic = String(TOPIC_CMD) + node_id;
    if (String(topic) == cmd_topic) {
        handleCommand(doc);
    }
}

void handleCommand(JsonDocument& cmd) {
    String command = cmd["command"];
    
    DEBUG_PRINT("[CMD] Received: ");
    DEBUG_PRINTLN(command);
    
    if (command == "calibrate") {
        DEBUG_PRINTLN("[CMD] Starting calibration...");
        performInitialCalibration();
        
        // Send confirmation
        StaticJsonDocument<256> response;
        response["command"] = "calibrate";
        response["status"] = is_calibrated ? "success" : "failed";
        response["gravity_mag"] = calibration.getData().gravity_mag;
        response["pitch_deg"] = calibration.getData().pitch_deg;
        response["roll_deg"] = calibration.getData().roll_deg;
        mqtt.publishStatus(node_id.c_str(), response);
        
    } else if (command == "restart") {
        DEBUG_PRINTLN("[CMD] Restarting...");
        delay(1000);
        ESP.restart();
        
    } else if (command == "status") {
        DEBUG_PRINTLN("[CMD] Sending status...");
        sendHeartbeat();  // Immediate heartbeat
        
    } else if (command == "update") {
        DEBUG_PRINTLN("[CMD] Manual OTA update requested...");

        // Send status
        StaticJsonDocument<256> response;
        response["command"] = "update";
        response["status"] = "checking";
        response["current_version"] = FIRMWARE_VERSION;
        mqtt.publishStatus(node_id.c_str(), response);

        // Check and install update
        if (ota.checkForUpdate()) {
            response["status"] = "updating";
            response["new_version"] = ota.getLatestVersion();
            mqtt.publishStatus(node_id.c_str(), response);

            ota.performUpdate();  // Will restart on success
        } else {
            response["status"] = "up_to_date";
            mqtt.publishStatus(node_id.c_str(), response);
        }

    } else if (command == "config") {
        DEBUG_PRINTLN("[CMD] Configuration update (not implemented yet)");
        
    } else {
        DEBUG_PRINT("[CMD] Unknown command: ");
        DEBUG_PRINTLN(command);
    }
}

// ============================================================================
// HEARTBEAT
// ============================================================================

void sendHeartbeat() {
    StaticJsonDocument<512> doc;
    
    doc["timestamp_us"] = (uint64_t)esp_timer_get_time();
    doc["sequence"] = sequence_number++;
    doc["uptime_s"] = millis() / 1000;
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["heap_free_kb"] = ESP.getFreeHeap() / 1024;
    doc["triggers_24h"] = triggers_24h;
    
    // Add calibration info
    if (is_calibrated) {
        CalibrationData cal = calibration.getData();
        doc["gravity_mag"] = cal.gravity_mag;
        doc["pitch_deg"] = cal.pitch_deg;
        doc["roll_deg"] = cal.roll_deg;
        doc["calibration_age_h"] = (millis() - cal.timestamp) / 3600000UL;
    } else {
        doc["gravity_mag"] = 0;
    }
    
    // Current RMS (last window)
    doc["rms_mg"] = trigger_detector.calculateRMS();
    
    mqtt.publishHeartbeat(node_id.c_str(), doc);
    
    #if DEBUG_MQTT
    DEBUG_PRINTF("[HB] Sent (seq=%u, RSSI=%d dBm)\n", sequence_number-1, WiFi.RSSI());
    #endif
}

// ============================================================================
// TRIGGER DATA TRANSMISSION
// ============================================================================

void sendTriggerData(TriggerInfo& info) {
    DEBUG_PRINTLN("[TX] Preparing trigger data...");
    
    // Create JSON document (large for waveform)
    DynamicJsonDocument doc(8192);
    
    doc["timestamp_us"] = info.timestamp_us;
    doc["sequence"] = sequence_number++;
    
    // Trigger info
    JsonObject trigger_info = doc.createNestedObject("trigger_info");
    trigger_info["rms_mg"] = info.rms_mg;
    trigger_info["kurtosis"] = info.kurtosis;
    trigger_info["duration_ms"] = info.duration_ms;
    trigger_info["spectral_ratio"] = info.spectral_ratio;
    trigger_info["peak_freq_hz"] = info.peak_freq_hz;
    
    // Waveform data
    JsonObject waveform = doc.createNestedObject("waveform");
    waveform["axis"] = "Z";
    waveform["sample_rate"] = SAMPLE_RATE_HZ;
    waveform["samples"] = BUFFER_SIZE_SAMPLES;
    waveform["format"] = "int16";
    waveform["scale_factor"] = ADXL_SCALE_FACTOR;
    
    // Compress and encode waveform
    String compressed = compressWaveform();
    waveform["data"] = compressed;
    
    // Send via MQTT
    DEBUG_PRINTF("[TX] Sending %d bytes...\n", measureJson(doc));
    
    if (mqtt.publishTrigger(node_id.c_str(), doc)) {
        DEBUG_PRINTLN("[TX] ✓ Trigger data sent successfully");
    } else {
        DEBUG_PRINTLN("[TX] ✗ Failed to send trigger data");
    }
}

// ============================================================================
// WAVEFORM COMPRESSION
// ============================================================================

String compressWaveform() {
    // For now, we'll use base64 encoding
    // More sophisticated compression (delta encoding, etc.) can be added later
    
    // Base64 encode the buffer
    String encoded;
    size_t output_length;
    
    // Simple base64 encoding
    const char* base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    uint8_t* bytes = (uint8_t*)waveform_buffer;
    size_t len = WAVEFORM_SIZE_BYTES;
    
    for (size_t i = 0; i < len; i += 3) {
        uint32_t b = (bytes[i] << 16) | 
                     ((i + 1 < len) ? (bytes[i + 1] << 8) : 0) | 
                     ((i + 2 < len) ? bytes[i + 2] : 0);
        
        encoded += base64_chars[(b >> 18) & 0x3F];
        encoded += base64_chars[(b >> 12) & 0x3F];
        encoded += (i + 1 < len) ? base64_chars[(b >> 6) & 0x3F] : '=';
        encoded += (i + 2 < len) ? base64_chars[b & 0x3F] : '=';
    }
    
    DEBUG_PRINTF("[COMPRESS] Original: %d bytes, Encoded: %d bytes\n", 
                WAVEFORM_SIZE_BYTES, encoded.length());
    
    return encoded;
}