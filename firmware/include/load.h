// Watches the current drawn by the stepper driver, to catch the arm working
// against something it should not be.
//
// What this can and cannot do is worth being precise about, because the
// obvious reading of "stall detection" is wrong for a chopper-driven stepper.
//
// A TB6600 regulates phase current, so the motor draws roughly its set
// current whether it is turning or not. What changes with load is the supply
// current, which tracks mechanical power: it climbs as the load torque rises
// toward the pull-out point, and then collapses once the rotor loses
// synchronism and stops doing useful work.
//
// So there are two signatures here, and both are heuristics:
//
//   Overload  sustained current above what moving the arm should need. The
//             arm is binding, jammed, or pushing against something. This is
//             the one that matters for safety.
//
//   Collapse  current falling to near its idle level while steps are still
//             being commanded. Consistent with the rotor having lost
//             synchronism. Also consistent with a disconnected motor.
//
// Neither is a substitute for a shaft encoder, which would make this exact
// rather than inferred. See docs/design.md.
#pragma once

#include <stdint.h>

class LoadMonitor {
public:
    void configure(float overload_amps, uint16_t overload_ms,
                   float collapse_amps, uint16_t collapse_ms,
                   uint16_t grace_ms, float filter_alpha) {
        overload_amps_ = overload_amps;
        overload_ms_   = overload_ms;
        collapse_amps_ = collapse_amps;
        collapse_ms_   = collapse_ms;
        grace_ms_      = grace_ms;
        filter_alpha_  = filter_alpha;
    }

    // Clears the trip and all timers, but keeps the filtered reading.
    void clear() {
        overload_ = false;
        collapse_ = false;
        over_since_ = 0;
        under_since_ = 0;
        moving_since_ = 0;
        was_moving_ = false;
    }

    void reset(float amps) {
        current_ = amps;
        clear();
    }

    void update(float amps, bool moving, uint32_t now_ms) {
        current_ += filter_alpha_ * (amps - current_);

        if (!moving) {
            // Idle current says nothing about load, and the driver may be
            // de-energised entirely.
            was_moving_ = false;
            over_since_ = 0;
            under_since_ = 0;
            return;
        }

        if (!was_moving_) {
            was_moving_ = true;
            moving_since_ = now_ms;
            over_since_ = 0;
            under_since_ = 0;
        }

        // Current takes time to settle after motion starts, and the inrush
        // at the first step would otherwise read as an overload.
        if ((now_ms - moving_since_) < grace_ms_) return;

        if (current_ > overload_amps_) {
            if (over_since_ == 0) over_since_ = now_ms;
            if ((now_ms - over_since_) >= overload_ms_) overload_ = true;
        } else {
            over_since_ = 0;
        }

        if (current_ < collapse_amps_) {
            if (under_since_ == 0) under_since_ = now_ms;
            if ((now_ms - under_since_) >= collapse_ms_) collapse_ = true;
        } else {
            under_since_ = 0;
        }
    }

    float current() const { return current_; }

    bool overload() const { return overload_; }
    bool collapse() const { return collapse_; }
    bool tripped() const { return overload_ || collapse_; }

private:
    float    overload_amps_ = 3.0f;
    uint16_t overload_ms_   = 400;
    float    collapse_amps_ = 0.15f;
    uint16_t collapse_ms_   = 600;
    uint16_t grace_ms_      = 250;
    float    filter_alpha_  = 0.05f;

    float current_ = 0.0f;

    bool overload_ = false;
    bool collapse_ = false;

    bool     was_moving_   = false;
    uint32_t moving_since_ = 0;
    uint32_t over_since_   = 0;
    uint32_t under_since_  = 0;
};
