// Watches a switch sited past the flexion soft limit, as a check on the two
// software limits rather than as a limit itself.
//
// The home switch at the other end is part of normal operation: the wearer
// reaches it every session and it sets zero. This one should never be touched.
// It sits one or two degrees beyond kMaxAngleDeg, so closing it means the
// planner failed to decelerate at the soft limit *and* the step generator's
// independent clamp failed to stop pulsing past it. Both of those are tested,
// which is exactly why something should be watching for them being wrong.
//
// That makes it the first physical check in this machine that catches a
// firmware fault, as opposed to catching a worn-out assumption. It is not a
// substitute for the mechanical hard stop behind it: a switch can only ask the
// firmware to stop, and the case it guards against is the firmware misbehaving.
// It is a layer, not the layer.
//
// Its second use is arithmetic. The switch is at a fixed place on the machine,
// so the step count when it closes measures the distance from the home switch
// to here. That span is the only number on this machine that can be compared
// against a physical length, which makes it the one available check on
// microstepping: if the TB6600's DIP switches say 1/8 while config.h says
// 1/4, every angle is wrong by a factor of two and the measured span says so.
// Nothing else in the firmware can detect that.
#pragma once

#include <stdint.h>

class OverrunGuard {
public:
    enum class Action : uint8_t {
        kNone,
        kOverrun,      // the soft limits were passed; span is meaningful
        kImplausible,  // closed somewhere it physically cannot be
    };

    // expected_steps: where the switch should be, counted from home.
    // tolerance_steps: how far the measured span may differ before saying so.
    // min_plausible_steps: below this, a closure says more about the wiring
    // than about the arm.
    void configure(long expected_steps, long tolerance_steps,
                   long min_plausible_steps) {
        expected_steps_      = expected_steps;
        tolerance_steps_     = tolerance_steps;
        min_plausible_steps_ = min_plausible_steps;
    }

    void reset() {
        was_closed_ = false;
        span_       = 0;
        measured_   = false;
    }

    Action update(bool switch_closed, long position) {
        const bool closing = switch_closed && !was_closed_;
        was_closed_ = switch_closed;

        if (!closing) return Action::kNone;

        // A switch cannot be at the flexion end of travel and also be reached
        // while the counter reads near zero. Either it is shorted, it is
        // wired to the wrong pin, or so many steps have been lost that the
        // span would be meaningless anyway. Fault either way, but do not
        // report the number as a measurement.
        if (position < min_plausible_steps_) {
            span_     = position;
            measured_ = false;
            return Action::kImplausible;
        }

        span_     = position;
        measured_ = true;
        return Action::kOverrun;
    }

    // Step count when the switch last closed. Only a measurement of the
    // machine when measured() is true.
    long span() const { return span_; }
    bool measured() const { return measured_; }

    // Signed difference from where the switch was expected. Positive means
    // the arm travelled further than the count predicted, which is what lost
    // steps or too-coarse microstepping both look like.
    long spanError() const { return span_ - expected_steps_; }

    bool spanSignificant() const {
        const long error = spanError();
        return error > tolerance_steps_ || error < -tolerance_steps_;
    }

private:
    long expected_steps_      = 0;
    long tolerance_steps_     = 0;
    long min_plausible_steps_ = 0;

    bool was_closed_ = false;
    long span_       = 0;
    bool measured_   = false;
};
