#include "qmi8658.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_log.h>

#include "shake_detector.h"

static_assert(ShakeDetector::kLsbPerG == 8192, "CTRL2 ±4g must match the shake scale");

namespace {
constexpr const char* kTag = "Qmi8658";
constexpr int kIoTimeoutMs = 50;

int16_t DecodeAxis(uint8_t low, uint8_t high) {
    return int16_t(uint16_t(low) | (uint16_t(high) << 8));
}
}  // namespace

Qmi8658::~Qmi8658() {
    if (device_ != nullptr) {
        i2c_master_bus_rm_device(device_);
        device_ = nullptr;
    }
}

bool Qmi8658::Init(i2c_master_bus_handle_t bus) {
    if (device_ != nullptr) {
        return true;
    }
    if (bus == nullptr) {
        return false;
    }
    uint8_t address = kAddress;
    if (i2c_master_probe(bus, kAddress, kIoTimeoutMs) != ESP_OK) {
        // SA0 strapped high would answer at 0x6B. This board's guide says 0x6A.
        if (i2c_master_probe(bus, 0x6B, kIoTimeoutMs) != ESP_OK) {
            ESP_LOGW(kTag, "QMI8658 not found at 0x6A; shake falls back to a long press");
            return false;
        }
        address = 0x6B;
        ESP_LOGW(kTag, "QMI8658 answered at 0x6B; the board guide expects 0x6A");
    }
    if (!Attach(bus, address)) {
        ESP_LOGW(kTag, "QMI8658 could not join the I2C bus");
        return false;
    }

    uint8_t who = 0;
    if (!Read(kRegWhoAmI, &who, 1) || who != kWhoAmI) {
        Detach("QMI8658 WHO_AM_I mismatch; shake falls back to a long press");
        return false;
    }
    if (!Write(kRegReset, 0xB0)) {
        Detach("QMI8658 reset failed");
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
    who = 0;
    bool identified = false;
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (Read(kRegWhoAmI, &who, 1) && who == kWhoAmI) {
            identified = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (!identified) {
        Detach("QMI8658 did not recover after reset");
        return false;
    }
    // Configure while the sensors are still off, then enable them.
    if (!Write(kRegCtrl1, kCtrl1AutoIncrement) || !Write(kRegCtrl2, kCtrl2Accel4g250Hz) ||
        !Write(kRegCtrl3, kCtrl3Gyro512dps250Hz) || !Write(kRegCtrl7, kCtrl7AccelAndGyro)) {
        Detach("QMI8658 configuration failed");
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    failures_ = 0;
    ESP_LOGI(kTag, "QMI8658 ready at 0x%02X, accel ±4g (%d LSB/g), gyro 512 dps", address,
             ShakeDetector::kLsbPerG);
    return true;
}

bool Qmi8658::ReadAccel(int16_t& x, int16_t& y, int16_t& z) {
    return ReadVector(kRegAxL, x, y, z);
}

bool Qmi8658::ReadGyro(int16_t& x, int16_t& y, int16_t& z) {
    return ReadVector(kRegGxL, x, y, z);
}

bool Qmi8658::Attach(i2c_master_bus_handle_t bus, uint8_t address) {
    i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = 400 * 1000,
        .scl_wait_us = 0,
        .flags =
            {
                .disable_ack_check = 0,
            },
    };
    device_ = nullptr;
    return i2c_master_bus_add_device(bus, &config, &device_) == ESP_OK && device_ != nullptr;
}

bool Qmi8658::Write(uint8_t reg, uint8_t value) {
    if (device_ == nullptr) {
        return false;
    }
    uint8_t buffer[2] = {reg, value};
    return i2c_master_transmit(device_, buffer, sizeof(buffer), kIoTimeoutMs) == ESP_OK;
}

bool Qmi8658::Read(uint8_t reg, uint8_t* data, size_t length) {
    if (device_ == nullptr || data == nullptr || length == 0) {
        return false;
    }
    return i2c_master_transmit_receive(device_, &reg, 1, data, length, kIoTimeoutMs) == ESP_OK;
}

bool Qmi8658::ReadVector(uint8_t reg, int16_t& x, int16_t& y, int16_t& z) {
    uint8_t raw[6] = {};
    if (!Read(reg, raw, sizeof(raw))) {
        if (device_ != nullptr && ++failures_ >= kMaxReadFailures) {
            Detach("QMI8658 stopped answering; shake falls back to a long press");
        }
        return false;
    }
    failures_ = 0;
    x = DecodeAxis(raw[0], raw[1]);
    y = DecodeAxis(raw[2], raw[3]);
    z = DecodeAxis(raw[4], raw[5]);
    return true;
}

void Qmi8658::Detach(const char* reason) {
    if (reason != nullptr) {
        ESP_LOGW(kTag, "%s", reason);
    }
    if (device_ != nullptr) {
        i2c_master_bus_rm_device(device_);
        device_ = nullptr;
    }
}
