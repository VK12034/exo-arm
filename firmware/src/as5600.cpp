#include "as5600.h"

#include <Wire.h>

namespace {

constexpr uint8_t kAddress    = 0x36;
constexpr uint8_t kRegStatus  = 0x0B;
constexpr uint8_t kRegRawHigh = 0x0C;

constexpr uint8_t kMagnetDetected = 0x20;
constexpr uint8_t kMagnetTooWeak  = 0x10;
constexpr uint8_t kMagnetTooStrong = 0x08;

}  // namespace

void As5600::begin() {
    Wire.begin();
    Wire.setClock(400000);

    // A stuck bus must not hang the loop, because the loop is what runs the
    // interlocks. Three milliseconds is far longer than a healthy read.
    Wire.setWireTimeout(3000, true);
}

bool As5600::readRegisters(uint8_t reg, uint8_t* buffer, uint8_t count) {
    Wire.beginTransmission(kAddress);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;

    if (Wire.requestFrom(kAddress, count) != count) return false;

    for (uint8_t i = 0; i < count; ++i) {
        buffer[i] = static_cast<uint8_t>(Wire.read());
    }
    return true;
}

bool As5600::read(uint16_t* raw) {
    uint8_t status = 0;
    bus_ok_ = readRegisters(kRegStatus, &status, 1);
    if (!bus_ok_) return false;

    status_ = status;
    if ((status & kMagnetDetected) == 0) return false;
    if ((status & (kMagnetTooWeak | kMagnetTooStrong)) != 0) return false;

    uint8_t bytes[2] = {0, 0};
    bus_ok_ = readRegisters(kRegRawHigh, bytes, 2);
    if (!bus_ok_) return false;

    *raw = static_cast<uint16_t>((static_cast<uint16_t>(bytes[0]) << 8) | bytes[1]) & 0x0FFF;
    return true;
}

const __FlashStringHelper* As5600::faultReason() const {
    if (!bus_ok_) return F("encoder not answering - check the I2C wiring");
    if ((status_ & kMagnetDetected) == 0) return F("no magnet at the encoder");
    if ((status_ & kMagnetTooWeak) != 0) return F("encoder magnet too far away");
    if ((status_ & kMagnetTooStrong) != 0) return F("encoder magnet too close");
    return F("encoder read failed");
}
