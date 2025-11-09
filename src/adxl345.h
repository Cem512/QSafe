#ifndef ADXL345_H
#define ADXL345_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"

// ADXL345 Register Map
#define ADXL345_REG_DEVID         0x00
#define ADXL345_REG_POWER_CTL     0x2D
#define ADXL345_REG_DATA_FORMAT   0x31
#define ADXL345_REG_BW_RATE       0x2C
#define ADXL345_REG_DATAX0        0x32
#define ADXL345_REG_DATAY0        0x34
#define ADXL345_REG_DATAZ0        0x36
#define ADXL345_REG_FIFO_CTL      0x38

// Commands
#define ADXL345_DEVID             0xE5
#define ADXL345_POWER_MEASURE     0x08
#define ADXL345_RATE_200HZ        0x0B  // 200 Hz output data rate

// I2C Settings
#define ADXL345_I2C_ADDR          0x53  // Default I2C address (SDO = GND)

class ADXL345 {
public:
    ADXL345();  // I2C constructor

    bool begin();
    bool testConnection();
    void readRaw(int16_t &x, int16_t &y, int16_t &z);
    void readAcceleration(float &x, float &y, float &z);  // Returns in g

    // Configuration
    void setRange(uint8_t range);  // 2, 4, 8, 16 (g)
    void setDataRate(uint8_t rate);
    void setScaleFactor(float scale_factor);  // Set calibrated scale factor (mg/LSB)

private:
    uint8_t _i2c_addr;
    float _scale_factor;  // mg per LSB

    uint8_t readRegister(uint8_t reg);
    void writeRegister(uint8_t reg, uint8_t value);
    void readRegisters(uint8_t reg, uint8_t *buffer, uint8_t len);
};

#endif // ADXL345_H