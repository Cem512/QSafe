#include "trigger.h"
#include <math.h>

TriggerDetector::TriggerDetector() {
    _window_index = 0;
    _window_full = false;
    _is_triggered = false;
    _trigger_start_time = 0;

    memset(_window_buffer, 0, sizeof(_window_buffer));

    // Initialize FFT object (using legacy API for v1.6.2)
    _fft = arduinoFFT(_vReal, _vImag, FFT_SIZE, SAMPLE_RATE_HZ);
}

void TriggerDetector::addSample(float value_mg) {
    _window_buffer[_window_index] = value_mg;
    _window_index++;
    
    if (_window_index >= FFT_SIZE) {
        _window_index = 0;
        _window_full = true;
    }
}

bool TriggerDetector::checkTrigger(TriggerInfo &info) {
    if (!_window_full) {
        return false;  // Not enough samples yet
    }
    
    // Step 1: Calculate RMS
    float rms = calculateRMS();
    
    if (rms < RMS_THRESHOLD_MG) {
        _is_triggered = false;
        return false;  // Below noise floor
    }
    
    // Step 2: Calculate Kurtosis (impulsiveness)
    float kurtosis = calculateKurtosis();
    
    if (kurtosis < KURTOSIS_THRESHOLD) {
        _is_triggered = false;
        return false;  // Not impulsive enough
    }
    
    // Step 3: Check spectral content (FFT-based noise rejection)
    if (!checkSpectralContent(info)) {
        _is_triggered = false;
        return false;  // Failed spectral check
    }
    
    // Step 4: Check duration
    if (!_is_triggered) {
        _is_triggered = true;
        _trigger_start_time = millis();
    }
    
    unsigned long duration = millis() - _trigger_start_time;
    
    if (duration < MIN_DURATION_MS) {
        return false;  // Too short (likely impact)
    }
    
    // ✓ All checks passed - valid trigger!
    info.rms_mg = rms;
    info.kurtosis = kurtosis;
    info.duration_ms = duration;
    info.timestamp_us = micros();
    info.is_valid = true;
    
    #if DEBUG_TRIGGER
    DEBUG_PRINTLN("\n[TRIGGER] ✓ EVENT DETECTED!");
    DEBUG_PRINTF("  Wave Type: %s\n", info.wave_type);
    DEBUG_PRINTF("  RMS: %.2f mg\n", info.rms_mg);
    DEBUG_PRINTF("  Kurtosis: %.2f\n", info.kurtosis);
    DEBUG_PRINTF("  Spectral Ratio: %.2f\n", info.spectral_ratio);
    DEBUG_PRINTF("  Peak Freq: %.1f Hz\n", info.peak_freq_hz);
    DEBUG_PRINTF("  P-wave Energy: %.1f | S-wave Energy: %.1f\n", info.p_wave_energy, info.s_wave_energy);
    DEBUG_PRINTF("  Duration: %lu ms\n", info.duration_ms);
    #endif
    
    return true;
}

float TriggerDetector::calculateRMS() {
    float sum_sq = 0;
    
    // Use kurtosis window size for RMS calculation
    int window_samples = (KURTOSIS_WINDOW_MS * SAMPLE_RATE_HZ) / 1000;
    window_samples = min(window_samples, FFT_SIZE);
    
    for (int i = 0; i < window_samples; i++) {
        int idx = (_window_index - window_samples + i + FFT_SIZE) % FFT_SIZE;
        float val = _window_buffer[idx];
        sum_sq += val * val;
    }
    
    return sqrt(sum_sq / window_samples);
}

float TriggerDetector::calculateKurtosis() {
    // Kurtosis measures "peakedness" - high for impulsive signals
    // Kurt = E[(X-μ)^4] / (E[(X-μ)^2])^2
    
    int window_samples = (KURTOSIS_WINDOW_MS * SAMPLE_RATE_HZ) / 1000;
    window_samples = min(window_samples, FFT_SIZE);
    
    // Calculate mean
    float mean = 0;
    for (int i = 0; i < window_samples; i++) {
        int idx = (_window_index - window_samples + i + FFT_SIZE) % FFT_SIZE;
        mean += _window_buffer[idx];
    }
    mean /= window_samples;
    
    // Calculate variance and 4th moment
    float m2 = 0;  // 2nd moment (variance)
    float m4 = 0;  // 4th moment
    
    for (int i = 0; i < window_samples; i++) {
        int idx = (_window_index - window_samples + i + FFT_SIZE) % FFT_SIZE;
        float diff = _window_buffer[idx] - mean;
        float diff_sq = diff * diff;
        m2 += diff_sq;
        m4 += diff_sq * diff_sq;
    }
    
    m2 /= window_samples;
    m4 /= window_samples;
    
    if (m2 < 1e-6) return 0;  // Avoid division by zero
    
    float kurtosis = m4 / (m2 * m2);
    
    return kurtosis;
}

bool TriggerDetector::checkSpectralContent(TriggerInfo &info) {
    // Copy buffer to FFT arrays
    for (int i = 0; i < FFT_SIZE; i++) {
        _vReal[i] = (double)_window_buffer[i];
        _vImag[i] = 0;
    }

    // Apply Hamming window (using legacy API for v1.6.2)
    _fft.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);

    // Compute FFT (using legacy API)
    _fft.Compute(FFT_FORWARD);
    _fft.ComplexToMagnitude();

    // Calculate energy in earthquake band (0.5-15 Hz)
    float eq_energy = 0;
    int eq_start_bin = (EARTHQUAKE_BAND_LOW * FFT_SIZE) / SAMPLE_RATE_HZ;
    int eq_end_bin = (EARTHQUAKE_BAND_HIGH * FFT_SIZE) / SAMPLE_RATE_HZ;

    for (int i = eq_start_bin; i <= eq_end_bin; i++) {
        eq_energy += _vReal[i];
    }

    // Calculate energy in noise band (15-50 Hz)
    float noise_energy = 0;
    int noise_start_bin = (NOISE_BAND_LOW * FFT_SIZE) / SAMPLE_RATE_HZ;
    int noise_end_bin = (NOISE_BAND_HIGH * FFT_SIZE) / SAMPLE_RATE_HZ;

    for (int i = noise_start_bin; i <= noise_end_bin; i++) {
        noise_energy += _vReal[i];
    }

    // Calculate P-wave band energy (4-15 Hz - higher frequency)
    float p_wave_energy = 0;
    int p_start_bin = (P_WAVE_BAND_LOW * FFT_SIZE) / SAMPLE_RATE_HZ;
    int p_end_bin = (P_WAVE_BAND_HIGH * FFT_SIZE) / SAMPLE_RATE_HZ;

    for (int i = p_start_bin; i <= p_end_bin; i++) {
        p_wave_energy += _vReal[i];
    }
    info.p_wave_energy = p_wave_energy;

    // Calculate S-wave band energy (1-8 Hz - lower frequency)
    float s_wave_energy = 0;
    int s_start_bin = (S_WAVE_BAND_LOW * FFT_SIZE) / SAMPLE_RATE_HZ;
    int s_end_bin = (S_WAVE_BAND_HIGH * FFT_SIZE) / SAMPLE_RATE_HZ;

    for (int i = s_start_bin; i <= s_end_bin; i++) {
        s_wave_energy += _vReal[i];
    }
    info.s_wave_energy = s_wave_energy;

    // Classify wave type based on energy distribution
    float p_s_ratio = (s_wave_energy > 0) ? (p_wave_energy / s_wave_energy) : 999.0;

    if (p_s_ratio > 1.5) {
        strcpy(info.wave_type, "P-wave");
    } else if (p_s_ratio < 0.67) {
        strcpy(info.wave_type, "S-wave");
    } else {
        strcpy(info.wave_type, "Mixed");
    }

    // Calculate spectral ratio (seismic vs noise)
    float spectral_ratio = (noise_energy > 0) ? (eq_energy / noise_energy) : 999.0;
    info.spectral_ratio = spectral_ratio;

    // Find peak frequency
    float max_magnitude = 0;
    int max_bin = 0;
    for (int i = eq_start_bin; i <= eq_end_bin; i++) {
        if (_vReal[i] > max_magnitude) {
            max_magnitude = _vReal[i];
            max_bin = i;
        }
    }
    info.peak_freq_hz = (max_bin * SAMPLE_RATE_HZ) / (float)FFT_SIZE;

    // Check if spectral ratio meets threshold
    if (spectral_ratio < SPECTRAL_RATIO_MIN) {
        #if DEBUG_TRIGGER
        DEBUG_PRINTF("[TRIGGER] Rejected: Spectral ratio %.2f < %.2f (high-freq noise)\n",
                    spectral_ratio, SPECTRAL_RATIO_MIN);
        #endif
        return false;
    }

    #if DEBUG_TRIGGER
    DEBUG_PRINTF("[TRIGGER] ✓ Accepted: %s detected (P/S ratio: %.2f, Peak: %.1f Hz)\n",
                info.wave_type, p_s_ratio, info.peak_freq_hz);
    #endif

    return true;
}

void TriggerDetector::reset() {
    _is_triggered = false;
    _trigger_start_time = 0;
}

float TriggerDetector::calculateMean(float* data, int len) {
    float sum = 0;
    for (int i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum / len;
}

float TriggerDetector::calculateVariance(float* data, int len, float mean) {
    float sum_sq = 0;
    for (int i = 0; i < len; i++) {
        float diff = data[i] - mean;
        sum_sq += diff * diff;
    }
    return sum_sq / len;
}