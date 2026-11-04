#include "safety.h"

void Safety::begin() {
    pinMode(cfg::kEStopPin, INPUT_PULLUP);
    pinMode(cfg::kDeadmanPin, INPUT_PULLUP);
    pinMode(cfg::kStatusLed, OUTPUT);
}

void Safety::raise(uint8_t flag) {
    faults_ |= flag;
}

bool Safety::check(Joint* joints, uint8_t count, uint32_t now_ms) {
    // The e-stop is a normally-closed loop to ground. A broken wire, a pulled
    // connector and a pressed button all read the same: open, meaning stop.
    estop_open_ = digitalRead(cfg::kEStopPin) == HIGH;
    deadman_    = digitalRead(cfg::kDeadmanPin) == LOW;

    if (estop_open_) raise(proto::kFaultEStop);

    for (uint8_t i = 0; i < count; ++i) {
        if (joints[i].driverFault())  raise(proto::kFaultDriver);
        if (joints[i].overCurrent())  raise(proto::kFaultOverCurrent);
        if (joints[i].atSoftLimit())  raise(proto::kFaultLimit);
    }

    // A powered mode requires a live host link. Idle does not, so the arm can
    // sit safely on the bench with nothing connected.
    if (mode_ != proto::Mode::kIdle && (now_ms - last_host_ms_) > cfg::kHostTimeoutMs) {
        raise(proto::kFaultHostTimeout);
    }

    const bool safe = (faults_ == proto::kFaultNone) &&
                      (mode_ != proto::Mode::kIdle) &&
                      deadman_;

    // Solid while driving, slow blink while idle, fast blink on a fault.
    if (faults_ != proto::kFaultNone) {
        digitalWrite(cfg::kStatusLed, (now_ms / 100) % 2);
    } else if (safe) {
        digitalWrite(cfg::kStatusLed, HIGH);
    } else {
        digitalWrite(cfg::kStatusLed, (now_ms / 500) % 2);
    }

    if (!safe) {
        for (uint8_t i = 0; i < count; ++i) {
            if (joints[i].enabled()) joints[i].disable();
        }
        if (faults_ != proto::kFaultNone) mode_ = proto::Mode::kIdle;
    }

    return safe;
}

void Safety::requestMode(proto::Mode mode) {
    // Refuse to leave idle while anything is latched. The host has to clear
    // faults deliberately, which means a human has looked at the arm.
    if (faults_ != proto::kFaultNone && mode != proto::Mode::kIdle) return;
    mode_ = mode;
}

bool Safety::clearFaults() {
    // Nothing is cleared while the cause is still present, otherwise the
    // host could sit in a loop clearing a held e-stop.
    if (digitalRead(cfg::kEStopPin) == HIGH) return false;

    faults_ = proto::kFaultNone;
    mode_   = proto::Mode::kIdle;
    return true;
}
