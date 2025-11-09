#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// FIRMWARE VERSION
// ============================================================================
#define FIRMWARE_VERSION "1.0.3"

// ============================================================================
// MQTT CONFIGURATION
// ============================================================================
#define MQTT_PRIMARY_BROKER   "2d3a5393d1184da09df490c130e6f541.s1.eu.hivemq.cloud"
#define MQTT_PRIMARY_PORT     8883  // TLS port

// Credentials are loaded from credentials.h (not committed to git)
// Copy credentials.h.example to credentials.h and fill in your values
#include "../credentials.h"  // Contains MQTT_USERNAME and MQTT_PASSWORD

// MQTT Topics (auto-generated with node_id)
#define TOPIC_REGISTER        "eew/register"
#define TOPIC_HEARTBEAT       "eew/heartbeat/"  // + node_id
#define TOPIC_RAW             "eew/raw/"        // + node_id
#define TOPIC_STATUS          "eew/status/"     // + node_id
#define TOPIC_CMD             "eew/cmd/"        // + node_id
#define TOPIC_ALERT           "eew/alert"

#define MQTT_QOS              1
#define MQTT_KEEPALIVE_S      30
#define MQTT_RECONNECT_DELAY  5000  // ms

// ============================================================================
// ADXL345 CONFIGURATION (I2C)
// ============================================================================
// Pin definitions for ESP32 DevKit (I2C)
#define ADXL_SDA_PIN    21  // I2C Data
#define ADXL_SCL_PIN    22  // I2C Clock

// ADXL345 Settings
#define SAMPLE_RATE_HZ        200
#define ADXL_RANGE            2      // ±2g range
#define ADXL_DATA_FORMAT      0x08   // Full resolution, ±2g

// ============================================================================
// WAVEFORM BUFFER
// ============================================================================
#define BUFFER_SIZE_SAMPLES   2000   // 10 seconds @ 200 Hz
#define PRE_TRIGGER_SAMPLES   200    // 1 second before trigger
#define AXIS_TO_USE           2      // 0=X, 1=Y, 2=Z (Z-axis vertical)

// Data format
typedef int16_t sample_t;  // 2 bytes per sample
#define WAVEFORM_SIZE_BYTES   (BUFFER_SIZE_SAMPLES * sizeof(sample_t))

// ============================================================================
// TRIGGER THRESHOLDS
// ============================================================================
#define RMS_THRESHOLD_MG      5.0    // Minimum acceleration in milli-g (increased to reduce false triggers)
#define KURTOSIS_THRESHOLD    5.0    // Impulsiveness threshold
#define KURTOSIS_WINDOW_MS    500    // 0.5 second window
#define MIN_DURATION_MS       500    // Reject events <0.5s
#define COOLDOWN_TIME_MS      30000  // 30 second cooldown after trigger

// Spectral filter (FFT-based noise rejection)
#define FFT_SIZE              512

// Seismic frequency bands (optimized for P-wave and S-wave detection)
#define EARTHQUAKE_BAND_LOW   0.5    // Hz - capture slower P-waves and regional events
#define EARTHQUAKE_BAND_HIGH  15.0   // Hz - include higher-frequency P-waves
#define NOISE_BAND_LOW        15.0   // Hz - mechanical noise, footsteps, etc.
#define NOISE_BAND_HIGH       50.0   // Hz - high-frequency anthropogenic noise
#define SPECTRAL_RATIO_MIN    2.0    // EQ/noise energy ratio (seismic must dominate)

// Peak frequency validation (reject low-frequency drift and building sway)
#define PEAK_FREQ_MIN         1.0    // Hz - reject anything below 1 Hz (building sway, drift)
#define PEAK_FREQ_MAX         15.0   // Hz - reject high-frequency noise

// Wave type identification bands (for P-wave vs S-wave classification)
#define P_WAVE_BAND_LOW       4.0    // Hz - P-waves typically 4-15 Hz
#define P_WAVE_BAND_HIGH      15.0   // Hz
#define S_WAVE_BAND_LOW       1.0    // Hz - S-waves typically 1-8 Hz
#define S_WAVE_BAND_HIGH      8.0    // Hz

// ============================================================================
// CALIBRATION
// ============================================================================
#define CALIBRATION_SAMPLES   1000   // 5 seconds @ 200 Hz
#define GRAVITY_NOMINAL       1.0    // g
#define GRAVITY_TOLERANCE     0.05   // ±5%
#define CALIBRATION_CHECK_HRS 1      // Hourly drift check
#define STILL_THRESHOLD_MG    1.0    // For auto-calibration
#define MAX_TILT_DEG          45.0   // Warn if node tilted >45°

// ============================================================================
// TIMING & NTP
// ============================================================================
// NTP will be configured to Pi4 IP after network setup
#define NTP_SERVER_DEFAULT    "pool.ntp.org"
#define NTP_UPDATE_INTERVAL   3600000  // 1 hour in ms
#define TIMEZONE_OFFSET_SEC   0        // UTC (adjust for your timezone)

// ============================================================================
// WIFI PROVISIONING
// ============================================================================
#define AP_SSID_PREFIX        "EEW-Setup-"  // Will append last 4 MAC digits
#define AP_PASSWORD           ""            // Open network for setup
#define PROVISION_TIMEOUT_S   300           // 5 minutes
#define WIFI_CONNECT_TIMEOUT  30000         // 30 seconds

// ============================================================================
// SYSTEM PARAMETERS
// ============================================================================
#define HEARTBEAT_INTERVAL_MS 60000   // 1 minute
#define WATCHDOG_TIMEOUT_S    60      // Reset if hung for 60s
#define SERIAL_BAUD_RATE      115200
#define LED_PIN               2       // Built-in LED (ESP32 DevKit)

// ============================================================================
// MEMORY & PERFORMANCE
// ============================================================================
#define MIN_FREE_HEAP_KB      20      // Warn if heap < 20KB
#define STACK_SIZE            8192    // Bytes for tasks

// ============================================================================
// SCALE FACTORS
// ============================================================================
// ADXL345 ±2g range: 256 LSB/g (10-bit mode) or 1024 LSB/g (13-bit full res)
// Using full resolution mode: 3.9 mg/LSB (nominal)
// Adjusted for actual sensor: 0.848g measured -> scale to 1.0g = 3.9 * (1.0/0.848) = 4.6
#define ADXL_SCALE_FACTOR     4.6     // mg per LSB (calibrated for this sensor)

// Convert mg to g
#define MG_TO_G(x)            ((x) / 1000.0)

// ============================================================================
// DEBUG FLAGS
// ============================================================================
#define DEBUG_SERIAL          1       // Enable serial debug output
#define DEBUG_SAMPLING        0       // Print every sample (very verbose!)
#define DEBUG_TRIGGER         1       // Print trigger details
#define DEBUG_CALIBRATION     1       // Print calibration info
#define DEBUG_MQTT            1       // Print MQTT messages

#if DEBUG_SERIAL
  #define DEBUG_PRINT(x)      Serial.print(x)
  #define DEBUG_PRINTLN(x)    Serial.println(x)
  #define DEBUG_PRINTF(...)   Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
#endif

#endif // CONFIG_H