#include "adxl345.h"

ADXL345::ADXL345() {
    _i2c_addr = ADXL345_I2C_ADDR;
    _scale_factor = ADXL_SCALE_FACTOR;  // Default, will be updated after calibration
}

bool ADXL345::begin() {
    // Initialize I2C
    Wire.begin(ADXL_SDA_PIN, ADXL_SCL_PIN);
    Wire.setClock(400000);  // 400kHz Fast Mode

    delay(10);  // Power-up time

    // Check device ID
    if (!testConnection()) {
        DEBUG_PRINTLN("[ADXL345] Device ID mismatch!");
        return false;
    }

    // Configure sensor
    // 1. Set data rate to 200 Hz
    writeRegister(ADXL345_REG_BW_RATE, ADXL345_RATE_200HZ);

    // 2. Set range to ±2g, full resolution
    writeRegister(ADXL345_REG_DATA_FORMAT, 0x08);  // Full res, ±2g

    // 3. Disable FIFO (we'll use streaming mode)
    writeRegister(ADXL345_REG_FIFO_CTL, 0x00);

    // 4. Enter measurement mode
    writeRegister(ADXL345_REG_POWER_CTL, ADXL345_POWER_MEASURE);

    delay(10);

    DEBUG_PRINTLN("[ADXL345] Initialized successfully (I2C mode)");
    DEBUG_PRINTF("[ADXL345] I2C Address: 0x%02X\n", _i2c_addr);
    DEBUG_PRINTF("[ADXL345] Sample rate: %d Hz\n", SAMPLE_RATE_HZ);
    DEBUG_PRINTF("[ADXL345] Range: ±%d g\n", ADXL_RANGE);

    return true;
}

bool ADXL345::testConnection() {
    uint8_t devid = readRegister(ADXL345_REG_DEVID);
    DEBUG_PRINTF("[ADXL345] Device ID: 0x%02X (expected 0x%02X)\n", devid, ADXL345_DEVID);
    return (devid == ADXL345_DEVID);
}

void ADXL345::readRaw(int16_t &x, int16_t &y, int16_t &z) {
    uint8_t buffer[6];
    readRegisters(ADXL345_REG_DATAX0, buffer, 6);

    // ADXL345 is 13-bit, left-justified in 16-bit registers
    x = (int16_t)((buffer[1] << 8) | buffer[0]);
    y = (int16_t)((buffer[3] << 8) | buffer[2]);
    z = (int16_t)((buffer[5] << 8) | buffer[4]);
}

void ADXL345::readAcceleration(float &x, float &y, float &z) {
    int16_t raw_x, raw_y, raw_z;
    readRaw(raw_x, raw_y, raw_z);

    // Convert to g (using calibrated scale factor)
    x = raw_x * _scale_factor / 1000.0;  // mg to g
    y = raw_y * _scale_factor / 1000.0;
    z = raw_z * _scale_factor / 1000.0;
}

void ADXL345::setScaleFactor(float scale_factor) {
    _scale_factor = scale_factor;
    DEBUG_PRINTF("[ADXL345] Scale factor updated to %.3f mg/LSB\n", _scale_factor);
}

void ADXL345::setRange(uint8_t range) {
    uint8_t format = readRegister(ADXL345_REG_DATA_FORMAT);
    format &= 0xFC;  // Clear range bits

    switch(range) {
        case 2:  format |= 0x00; break;
        case 4:  format |= 0x01; break;
        case 8:  format |= 0x02; break;
        case 16: format |= 0x03; break;
        default: format |= 0x00; break;
    }

    writeRegister(ADXL345_REG_DATA_FORMAT, format);
}

void ADXL345::setDataRate(uint8_t rate) {
    writeRegister(ADXL345_REG_BW_RATE, rate);
}

// ============================================================================
// Private Methods - I2C Communication
// ============================================================================

uint8_t ADXL345::readRegister(uint8_t reg) {
    Wire.beginTransmission(_i2c_addr);
    Wire.write(reg);
    Wire.endTransmission();

    Wire.requestFrom(_i2c_addr, (uint8_t)1);
    if (Wire.available()) {
        return Wire.read();
    }
    return 0;
}

void ADXL345::writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(_i2c_addr);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

void ADXL345::readRegisters(uint8_t reg, uint8_t *buffer, uint8_t len) {
    Wire.beginTransmission(_i2c_addr);
    Wire.write(reg);
    Wire.endTransmission();

    Wire.requestFrom(_i2c_addr, len);
    for (uint8_t i = 0; i < len; i++) {
        if (Wire.available()) {
            buffer[i] = Wire.read();
        } else {
            buffer[i] = 0;
        }
    }
}
