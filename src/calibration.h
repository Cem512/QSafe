#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

struct CalibrationData {
    // Rotation matrix to align gravity to [0, 0, -1]
    float rotation[3][3];

    // Original gravity vector during calibration
    float gravity_x;
    float gravity_y;
    float gravity_z;
    float gravity_mag;

    // Orientation (derived from gravity vector)
    float pitch_deg;  // Rotation around Y-axis
    float roll_deg;   // Rotation around X-axis

    // Auto-calibrated scale factor (mg per LSB)
    float scale_factor;

    // Metadata
    unsigned long timestamp;  // When calibration was performed
    bool valid;
};

class Calibration {
public:
    Calibration();

    bool performCalibration(int16_t* raw_samples_x, int16_t* raw_samples_y, int16_t* raw_samples_z, int num_samples);
    bool loadFromNVS();
    bool saveToNVS();

    void applyRotation(float raw_x, float raw_y, float raw_z,
                      float &corrected_x, float &corrected_y, float &corrected_z);

    bool isValid() { return _cal_data.valid; }
    CalibrationData getData() { return _cal_data; }
    float getScaleFactor() { return _cal_data.scale_factor; }

    bool checkDrift(float current_gravity_mag);

private:
    CalibrationData _cal_data;
    Preferences _prefs;

    void computeRotationMatrix(float gx, float gy, float gz);
    void computeOrientation(float gx, float gy, float gz);
    float calculateMean(float* data, int len);
    float calculateMeanRaw(int16_t* data, int len);
    float calculateStdDev(float* data, int len, float mean);
};

#endif // CALIBRATION_H