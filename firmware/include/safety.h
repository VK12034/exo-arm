// Supervisory state machine. Owns the decision about whether the joints are
// allowed to drive at all, and latches faults until the host clears them.
#pragma once

#include <Arduino.h>

#include "config.h"
#include "joint.h"
#include "protocol.h"

class Safety {
public:
    void begin();

    // Evaluates every interlock and returns true while it is safe to drive.
    // Call once per control step, before the joints are updated.
    bool check(Joint* joints, uint8_t count, uint32_t now_ms);

    // The host must be heard from regularly while a powered mode is active.
    void noteHostMessage(uint32_t now_ms) { last_host_ms_ = now_ms; }

    void requestMode(proto::Mode mode);
    proto::Mode mode() const { return mode_; }

    // Clears latched faults, but refuses while the cause is still present.
    bool clearFaults();

    uint8_t faults() const { return faults_; }
    bool    deadmanHeld() const { return deadman_; }

private:
    void raise(uint8_t flag);

    proto::Mode mode_   = proto::Mode::kIdle;
    uint8_t     faults_ = proto::kFaultNone;

    bool     deadman_      = false;
    bool     estop_open_   = false;
    uint32_t last_host_ms_ = 0;
};
