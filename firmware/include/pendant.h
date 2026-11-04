// The hand pendant: UP, DOWN and a latching normally-closed e-stop.
#pragma once

#include <Arduino.h>

#include "config.h"
#include "debounce.h"
#include "motion.h"

class Pendant {
public:
    void begin(uint32_t now_ms);
    void poll(uint32_t now_ms);

    // What the buttons are asking for. Both held, or neither, means hold:
    // an ambiguous input must never be guessed at.
    motion::Command command() const;

    bool upHeld() const { return up_.state(); }
    bool downHeld() const { return down_.state(); }

    // The e-stop loop is normally closed to ground. A pressed mushroom, an
    // unplugged pendant and a severed cable all read the same: open.
    bool eStopOpen() const { return estop_open_; }

    // How long both buttons have been held together, used for the deliberate
    // re-zero gesture. Zero whenever they are not both down.
    uint32_t bothHeldMs(uint32_t now_ms) const;

private:
    Debounced up_;
    Debounced down_;
    Debounced estop_;

    bool     estop_open_ = true;
    uint32_t both_since_ = 0;
    bool     both_held_  = false;
};
