#include "calibration.h"
#include <math.h>

Calibration::Calibration() {
    _cal_data.valid = false;
    _cal_data.scale_factor = ADXL_SCALE_FACTOR;  // Default fallback
}

bool Calibration::performCalibration(int16_t* raw_samples_x, int16_t* raw_samples_y, int16_t* raw_samples_z, int num_samples) {
    DEBUG_PRINTLN("[CAL] Starting calibration...");

    // Step 1: Calculate mean of RAW sensor values (LSB units)
    float raw_mean_x = calculateMeanRaw(raw_samples_x, num_samples);
    float raw_mean_y = calculateMeanRaw(raw_samples_y, num_samples);
    float raw_mean_z = calculateMeanRaw(raw_samples_z, num_samples);

    DEBUG_PRINTF("[CAL] Raw mean: X=%.1f Y=%.1f Z=%.1f LSB\n", raw_mean_x, raw_mean_y, raw_mean_z);

    // Step 2: Calculate raw gravity magnitude (in LSB units)
    float raw_gravity_mag_lsb = sqrt(raw_mean_x*raw_mean_x + raw_mean_y*raw_mean_y + raw_mean_z*raw_mean_z);

    DEBUG_PRINTF("[CAL] Raw gravity magnitude: %.1f LSB\n", raw_gravity_mag_lsb);

    // Step 3: Calculate scale factor dynamically
    // We know the true gravity magnitude is 1.0g = 1000 mg
    // Scale factor (mg/LSB) = 1000 mg / raw_gravity_mag_lsb
    _cal_data.scale_factor = 1000.0 / raw_gravity_mag_lsb;

    DEBUG_PRINTF("[CAL] ✓ Auto-calibrated scale factor: %.3f mg/LSB\n", _cal_data.scale_factor);

    // Step 4: Now convert raw samples to g using the calibrated scale factor
    // Allocate temporary arrays for scaled values
    float* samples_x = (float*)malloc(num_samples * sizeof(float));
    float* samples_y = (float*)malloc(num_samples * sizeof(float));
    float* samples_z = (float*)malloc(num_samples * sizeof(float));

    if (!samples_x || !samples_y || !samples_z) {
        DEBUG_PRINTLN("[CAL] ✗ Memory allocation failed!");
        free(samples_x); free(samples_y); free(samples_z);
        return false;
    }

    // Convert raw to g using calibrated scale factor
    for (int i = 0; i < num_samples; i++) {
        samples_x[i] = raw_samples_x[i] * _cal_data.scale_factor / 1000.0;
        samples_y[i] = raw_samples_y[i] * _cal_data.scale_factor / 1000.0;
        samples_z[i] = raw_samples_z[i] * _cal_data.scale_factor / 1000.0;
    }

    // Step 5: Calculate mean gravity vector (in g)
    float mean_x = calculateMean(samples_x, num_samples);
    float mean_y = calculateMean(samples_y, num_samples);
    float mean_z = calculateMean(samples_z, num_samples);

    DEBUG_PRINTF("[CAL] Mean gravity: X=%.3f Y=%.3f Z=%.3f g\n", mean_x, mean_y, mean_z);

    // Step 6: Verify stillness (low standard deviation)
    float std_x = calculateStdDev(samples_x, num_samples, mean_x);
    float std_y = calculateStdDev(samples_y, num_samples, mean_y);
    float std_z = calculateStdDev(samples_z, num_samples, mean_z);

    float max_std = max(max(std_x, std_y), std_z);
    DEBUG_PRINTF("[CAL] Std dev: X=%.4f Y=%.4f Z=%.4f g\n", std_x, std_y, std_z);

    // Relaxed threshold for calibration - allow up to 10mg std dev (10x threshold)
    if (max_std > MG_TO_G(STILL_THRESHOLD_MG) * 10) {
        DEBUG_PRINTLN("[CAL] ERROR: Node is moving (high std dev)");
        free(samples_x); free(samples_y); free(samples_z);
        return false;
    }

    // Step 7: Calculate gravity magnitude (should be close to 1.0g now)
    float gravity_mag = sqrt(mean_x*mean_x + mean_y*mean_y + mean_z*mean_z);
    DEBUG_PRINTF("[CAL] Gravity magnitude: %.3f g\n", gravity_mag);

    if (fabs(gravity_mag - GRAVITY_NOMINAL) > GRAVITY_TOLERANCE) {
        DEBUG_PRINTF("[CAL] ERROR: Invalid gravity (%.3f g, expected %.2f±%.2f)\n",
                    gravity_mag, GRAVITY_NOMINAL, GRAVITY_TOLERANCE);
        free(samples_x); free(samples_y); free(samples_z);
        return false;
    }

    // Step 8: Store gravity vector
    _cal_data.gravity_x = mean_x;
    _cal_data.gravity_y = mean_y;
    _cal_data.gravity_z = mean_z;
    _cal_data.gravity_mag = gravity_mag;

    // Step 9: Compute rotation matrix
    computeRotationMatrix(mean_x, mean_y, mean_z);

    // Step 10: Compute orientation angles
    computeOrientation(mean_x, mean_y, mean_z);

    // Step 11: Mark as valid and save
    _cal_data.timestamp = millis();
    _cal_data.valid = true;

    DEBUG_PRINTLN("[CAL] ✓ Calibration successful!");
    DEBUG_PRINTF("[CAL] Orientation: Pitch=%.1f° Roll=%.1f°\n",
                _cal_data.pitch_deg, _cal_data.roll_deg);

    if (_cal_data.pitch_deg > MAX_TILT_DEG || _cal_data.roll_deg > MAX_TILT_DEG) {
        DEBUG_PRINTLN("[CAL] ⚠ WARNING: Node is highly tilted (>45°)");
    }

    saveToNVS();

    free(samples_x);
    free(samples_y);
    free(samples_z);

    return true;
}

void Calibration::computeRotationMatrix(float gx, float gy, float gz) {
    // Simplified rotation matrix to align gravity [gx, gy, gz] to [0, 0, -g]
    // This is a basic implementation - can be improved with quaternions
    
    float g_mag = sqrt(gx*gx + gy*gy + gz*gz);
    
    // Normalize gravity vector
    float gx_norm = gx / g_mag;
    float gy_norm = gy / g_mag;
    float gz_norm = gz / g_mag;
    
    // For now, store identity matrix (rotation applied conceptually)
    // In practice, we just subtract the gravity offset
    // Full rotation matrix implementation can be added later if needed
    
    _cal_data.rotation[0][0] = 1; _cal_data.rotation[0][1] = 0; _cal_data.rotation[0][2] = 0;
    _cal_data.rotation[1][0] = 0; _cal_data.rotation[1][1] = 1; _cal_data.rotation[1][2] = 0;
    _cal_data.rotation[2][0] = 0; _cal_data.rotation[2][1] = 0; _cal_data.rotation[2][2] = 1;
    
    // Store normalized gravity for offset correction
    // (We'll subtract this from future readings)
}

void Calibration::computeOrientation(float gx, float gy, float gz) {
    // Calculate pitch and roll from gravity vector
    // Pitch: rotation around Y-axis (forward/backward tilt)
    // Roll: rotation around X-axis (left/right tilt)
    
    float g_mag = sqrt(gx*gx + gy*gy + gz*gz);
    
    // Pitch: angle between gravity projection on XZ plane and Z-axis
    _cal_data.pitch_deg = atan2(gx, sqrt(gy*gy + gz*gz)) * 180.0 / PI;
    
    // Roll: angle between gravity projection on YZ plane and Z-axis
    _cal_data.roll_deg = atan2(gy, gz) * 180.0 / PI;
}

void Calibration::applyRotation(float raw_x, float raw_y, float raw_z,
                               float &corrected_x, float &corrected_y, float &corrected_z) {
    if (!_cal_data.valid) {
        // No calibration - pass through
        corrected_x = raw_x;
        corrected_y = raw_y;
        corrected_z = raw_z;
        return;
    }
    
    // Simple offset correction (subtract calibrated gravity, then add nominal gravity on Z)
    // This assumes we only care about Z-axis for earthquake detection
    
    // Remove the static gravity component
    corrected_x = raw_x - _cal_data.gravity_x;
    corrected_y = raw_y - _cal_data.gravity_y;
    corrected_z = raw_z - _cal_data.gravity_z;
    
    // Add back nominal gravity on Z-axis (so Z = -1g at rest)
    corrected_z += -_cal_data.gravity_mag;
    
    // Full rotation matrix multiply would go here if needed:
    // corrected = rotation_matrix * (raw - gravity_offset)
}

bool Calibration::checkDrift(float current_gravity_mag) {
    if (!_cal_data.valid) return false;
    
    float drift = fabs(current_gravity_mag - _cal_data.gravity_mag);
    float drift_pct = drift / _cal_data.gravity_mag * 100.0;
    
    if (drift_pct > GRAVITY_TOLERANCE * 100.0) {
        DEBUG_PRINTF("[CAL] ⚠ Drift detected: %.1f%% (%.3fg -> %.3fg)\n", 
                    drift_pct, _cal_data.gravity_mag, current_gravity_mag);
        return true;  // Needs recalibration
    }
    
    return false;
}

bool Calibration::loadFromNVS() {
    _prefs.begin("eew", true);  // Read-only

    bool valid = _prefs.getBool("cal_valid", false);

    if (!valid) {
        DEBUG_PRINTLN("[CAL] No calibration found in NVS");
        _prefs.end();
        return false;
    }

    _cal_data.valid = true;
    _cal_data.gravity_x = _prefs.getFloat("cal_gx", 0);
    _cal_data.gravity_y = _prefs.getFloat("cal_gy", 0);
    _cal_data.gravity_z = _prefs.getFloat("cal_gz", 0);
    _cal_data.gravity_mag = _prefs.getFloat("cal_gmag", 1.0);
    _cal_data.pitch_deg = _prefs.getFloat("cal_pitch", 0);
    _cal_data.roll_deg = _prefs.getFloat("cal_roll", 0);
    _cal_data.scale_factor = _prefs.getFloat("cal_scale", ADXL_SCALE_FACTOR);  // Load scale factor
    _cal_data.timestamp = _prefs.getULong("cal_time", 0);

    _prefs.end();

    DEBUG_PRINTLN("[CAL] ✓ Loaded calibration from NVS");
    DEBUG_PRINTF("[CAL] Gravity: %.3fg, Pitch: %.1f°, Roll: %.1f°\n",
                _cal_data.gravity_mag, _cal_data.pitch_deg, _cal_data.roll_deg);
    DEBUG_PRINTF("[CAL] Scale factor: %.3f mg/LSB\n", _cal_data.scale_factor);

    return true;
}

bool Calibration::saveToNVS() {
    _prefs.begin("eew", false);  // Read-write

    _prefs.putBool("cal_valid", _cal_data.valid);
    _prefs.putFloat("cal_gx", _cal_data.gravity_x);
    _prefs.putFloat("cal_gy", _cal_data.gravity_y);
    _prefs.putFloat("cal_gz", _cal_data.gravity_z);
    _prefs.putFloat("cal_gmag", _cal_data.gravity_mag);
    _prefs.putFloat("cal_pitch", _cal_data.pitch_deg);
    _prefs.putFloat("cal_roll", _cal_data.roll_deg);
    _prefs.putFloat("cal_scale", _cal_data.scale_factor);  // Save scale factor
    _prefs.putULong("cal_time", _cal_data.timestamp);

    _prefs.end();

    DEBUG_PRINTLN("[CAL] ✓ Saved calibration to NVS");

    return true;
}

// ============================================================================
// Helper Functions
// ============================================================================

float Calibration::calculateMean(float* data, int len) {
    float sum = 0;
    for (int i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum / len;
}

float Calibration::calculateMeanRaw(int16_t* data, int len) {
    float sum = 0;
    for (int i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum / len;
}

float Calibration::calculateStdDev(float* data, int len, float mean) {
    float sum_sq = 0;
    for (int i = 0; i < len; i++) {
        float diff = data[i] - mean;
        sum_sq += diff * diff;
    }
    return sqrt(sum_sq / len);
}