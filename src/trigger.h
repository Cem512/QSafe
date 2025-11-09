#ifndef TRIGGER_H
#define TRIGGER_H

#include <Arduino.h>
#include "config.h"
#include "arduinoFFT.h"

struct TriggerInfo {
    float rms_mg;
    float kurtosis;
    float spectral_ratio;
    float peak_freq_hz;
    unsigned long duration_ms;
    unsigned long timestamp_us;
    bool is_valid;

    // Wave type identification
    char wave_type[8];  // "P-wave", "S-wave", or "Mixed"
    float p_wave_energy;
    float s_wave_energy;
};

class TriggerDetector {
public:
    TriggerDetector();
    
    void addSample(float value_mg);
    bool checkTrigger(TriggerInfo &info);
    
    float calculateRMS();
    float calculateKurtosis();
    bool checkSpectralContent(TriggerInfo &info);
    
    void reset();
    
private:
    float _window_buffer[FFT_SIZE];
    int _window_index;
    bool _window_full;
    
    unsigned long _trigger_start_time;
    bool _is_triggered;
    
    // ArduinoFFT object (using legacy API for v1.6.2)
    arduinoFFT _fft;
    double _vReal[FFT_SIZE];
    double _vImag[FFT_SIZE];
    
    float calculateMean(float* data, int len);
    float calculateVariance(float* data, int len, float mean);
};

#endif // TRIGGER_H