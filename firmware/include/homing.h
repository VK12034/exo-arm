// Establishes where full extension actually is, using a switch at the end of
// travel, so the soft limits stop resting on an assumption.
//
// Without a switch, position is counted steps from whatever pose the arm
// happened to be in at power-on. Park it wrong and every limit is wrong with
// it, in the same direction, by the same amount, with nothing to say so. That
// is the gap this closes.
//
// Two things this deliberately does not do.
//
// It does not home by itself. The machine's one rule is that it only moves
// while a human is holding a button, and a homing routine that drives the arm
// at power-on would break that rule to fix a lesser problem. Instead the zero
// is adopted the moment the wearer extends the arm far enough to reach the
// switch, which is a motion they were commanding anyway. Until that happens
// the arm is "unhomed" and the caller restricts travel accordingly.
//
// It does not trust the counter over the switch. The switch is a physical
// feature of the machine; the counter is arithmetic that drifts every time a
// step is lost. Where they disagree the switch wins. A small disagreement is
// corrected silently, because the counter rounds. A large one means steps
// were lost, which also means the far limit is no longer where it is
// believed to be, so it faults rather than quietly carrying on.
#pragma once

#include <stdint.h>

class Homing {
public:
    // What the caller should do about this update.
    enum class Action : uint8_t {
        kNone,
        kZero,   // adopt the switch as the zero reference
        kFault,  // switch and counter disagree by more than can be trusted
    };

    // How the current zero came to be believed, which is worth reporting
    // because the two are not equally trustworthy.
    enum class Source : uint8_t {
        kNone,      // unhomed: the counter is an assumption
        kSwitch,    // measured against a physical feature of the machine
        kOperator,  // asserted by the re-zero gesture, for builds with no switch
    };

    void configure(long home_steps, long tolerance_steps, long fault_steps) {
        home_steps_      = home_steps;
        tolerance_steps_ = tolerance_steps;
        fault_steps_     = fault_steps;
    }

    void reset() {
        homed_      = false;
        source_     = Source::kNone;
        drift_      = 0;
        was_closed_ = false;
    }

    // Records that the operator has asserted this pose is full extension.
    // Weaker than reaching the switch, and reported as such.
    void adoptOperatorZero() {
        homed_  = true;
        source_ = Source::kOperator;
        drift_  = 0;
    }

    Action update(bool switch_closed, long position) {
        const bool closing = switch_closed && !was_closed_;
        was_closed_ = switch_closed;

        // While unhomed the counter means nothing, so any contact with the
        // switch is the reference, not a discrepancy. Contact is tested rather
        // than the edge so that an arm correctly parked against the switch at
        // power-on homes on the first update instead of having to leave the
        // switch and come back.
        if (!homed_) {
            if (!switch_closed) return Action::kNone;

            drift_  = 0;
            homed_  = true;
            source_ = Source::kSwitch;
            return Action::kZero;
        }

        if (closing) {
            drift_ = position - home_steps_;

            // Beyond this the counter has lost enough steps that the far
            // limit cannot be trusted either. Correcting the zero would hide
            // that, so fall back to unhomed and let the caller fault.
            if (drift_ > fault_steps_ || drift_ < -fault_steps_) {
                homed_  = false;
                source_ = Source::kNone;
                return Action::kFault;
            }

            // The switch is the better reference even if a zero was already
            // believed, so reaching it takes over from an asserted one.
            source_ = Source::kSwitch;
            return Action::kZero;
        }

        // The switch reads closed while the counter says the arm is well into
        // flexion. A normally-open switch cannot do that by being broken, only
        // by being shorted or jammed shut, and a jammed home switch would
        // keep re-zeroing the arm mid-travel.
        if (switch_closed && (position - home_steps_) > fault_steps_) {
            return Action::kFault;
        }

        return Action::kNone;
    }

    bool   homed() const { return homed_; }
    Source source() const { return source_; }

    // Signed discrepancy measured the last time the switch was reached, in
    // steps. Positive means the counter was reading high: steps were lost and
    // the arm was further extended than it claimed.
    long drift() const { return drift_; }

    // Whether the last correction was large enough to be worth reporting but
    // small enough to apply.
    bool driftSignificant() const {
        return drift_ > tolerance_steps_ || drift_ < -tolerance_steps_;
    }

private:
    long home_steps_      = 0;
    long tolerance_steps_ = 0;
    long fault_steps_     = 0;

    bool   homed_      = false;
    Source source_     = Source::kNone;
    long   drift_      = 0;
    bool   was_closed_ = false;
};
