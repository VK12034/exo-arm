// AS5600 magnetic rotary encoder on I2C, over a diametric magnet on the elbow
// axis.
#pragma once

#include <Arduino.h>

class As5600 {
public:
    void begin();

    // Reads the 12-bit raw angle. Returns false on a bus error or if the magnet
    // is missing or badly placed, because the sensor still returns a number in
    // those cases and the number is meaningless.
    bool read(uint16_t* raw);

    // Why the last read failed, for the fault message.
    const __FlashStringHelper* faultReason() const;

private:
    bool readRegisters(uint8_t reg, uint8_t* buffer, uint8_t count);

    uint8_t status_ = 0;
    bool    bus_ok_ = false;
};
