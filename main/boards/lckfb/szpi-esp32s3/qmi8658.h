#pragma once

#include <cstdint>

#include <driver/i2c_master.h>

#include "accel_source.h"

// QMI8658C on the shared I2C bus of the LCKFB 立创实战派 ESP32-S3.
// The board guide uses 7-bit address 0x6A (SDA GPIO1, SCL GPIO2), the same
// bus as the codecs, the PCA9557, and the touch controller.
// https://wiki.lckfb.com/zh-hans/szpi-esp32s3/beginner/attitude-sensor.html
class Qmi8658 : public AccelSource {
public:
    static constexpr uint8_t kAddress = 0x6A;
    static constexpr uint8_t kWhoAmI = 0x05;

    Qmi8658() = default;
    ~Qmi8658() override;
    Qmi8658(const Qmi8658&) = delete;
    Qmi8658& operator=(const Qmi8658&) = delete;

    // Probe and configure. A missing chip returns false and leaves the bus alone.
    bool Init(i2c_master_bus_handle_t bus);
    bool available() const override { return device_ != nullptr; }
    bool ReadAccel(int16_t& x, int16_t& y, int16_t& z) override;
    bool ReadGyro(int16_t& x, int16_t& y, int16_t& z);

private:
    static constexpr uint8_t kRegWhoAmI = 0x00;
    static constexpr uint8_t kRegCtrl1 = 0x02;
    static constexpr uint8_t kRegCtrl2 = 0x03;
    static constexpr uint8_t kRegCtrl3 = 0x04;
    static constexpr uint8_t kRegCtrl7 = 0x08;
    static constexpr uint8_t kRegAxL = 0x35;
    static constexpr uint8_t kRegGxL = 0x3B;
    static constexpr uint8_t kRegReset = 0x60;
    // Vendor sequence for this board: auto-increment, ±4g at 250 Hz,
    // gyro 512 dps at 250 Hz, then enable accel and gyro.
    static constexpr uint8_t kCtrl1AutoIncrement = 0x40;
    static constexpr uint8_t kCtrl2Accel4g250Hz = 0x95;
    static constexpr uint8_t kCtrl3Gyro512dps250Hz = 0xD5;
    static constexpr uint8_t kCtrl7AccelAndGyro = 0x03;
    static constexpr int kMaxReadFailures = 8;

    i2c_master_dev_handle_t device_ = nullptr;
    int failures_ = 0;

    bool Attach(i2c_master_bus_handle_t bus, uint8_t address);
    bool Write(uint8_t reg, uint8_t value);
    bool Read(uint8_t reg, uint8_t* data, size_t length);
    bool ReadVector(uint8_t reg, int16_t& x, int16_t& y, int16_t& z);
    void Detach(const char* reason);
};
