// Compares the measured elbow angle with where the step counter thinks the arm
// is, which turns lost steps, chain slip and back-driving from guesses into
// measurements.
//
// The sensor sits on the elbow axis rather than the motor, so everything in the
// drivetrain is inside the measurement. Its zero is captured when the arm homes,
// which means no calibration constant to get wrong: the switch defines zero for
// both the counter and the encoder, and only the difference matters afterwards.
//
// Arithmetic only, with no I2C and no Arduino headers, so the tests can drive it
// with synthetic readings.
#pragma once

#include <stdint.h>

class AngleTracker {
public:
    void configure(uint16_t counts_per_rev, bool invert, float tolerance_deg, uint16_t hold_ms) {
        counts_per_rev_ = counts_per_rev;
        invert_ = invert;
        tolerance_deg_ = tolerance_deg;
        hold_ms_ = hold_ms;
    }

    // Called when the arm homes. Until then there is nothing to compare against.
    void setZero(uint16_t raw) {
        zero_ = raw;
        zeroed_ = true;
        angle_ = 0.0f;
        error_ = 0.0f;
        slipping_ = false;
        over_ = false;
    }

    void clearZero() {
        zeroed_ = false;
        slipping_ = false;
        over_ = false;
        error_ = 0.0f;
    }

    // Degrees from zero. Travel is far less than a turn, so the result is the
    // shorter way round, which keeps it continuous if the raw reading rolls
    // over inside the travel.
    float toDegrees(uint16_t raw) const {
        int32_t delta = static_cast<int32_t>(raw) - static_cast<int32_t>(zero_);
        const int32_t span = static_cast<int32_t>(counts_per_rev_);

        while (delta > span / 2) delta -= span;
        while (delta < -(span / 2)) delta += span;

        const float degrees = (static_cast<float>(delta) * 360.0f) / static_cast<float>(span);
        return invert_ ? -degrees : degrees;
    }

    // Checked whether or not the arm is being driven: a growing error while
    // stopped means the drivetrain is giving way.
    void update(uint16_t raw, float commanded_deg, uint32_t now_ms) {
        if (!zeroed_) return;

        angle_ = toDegrees(raw);
        error_ = angle_ - commanded_deg;

        const float magnitude = error_ < 0.0f ? -error_ : error_;

        if (magnitude > tolerance_deg_) {
            if (!over_) {
                over_ = true;
                over_since_ = now_ms;
            }
            if ((now_ms - over_since_) >= hold_ms_) slipping_ = true;
        } else {
            over_ = false;
        }
    }

    bool  zeroed() const { return zeroed_; }
    bool  slipping() const { return slipping_; }
    float angle() const { return angle_; }
    float error() const { return error_; }

private:
    uint16_t counts_per_rev_ = 4096;
    bool     invert_ = false;
    float    tolerance_deg_ = 3.0f;
    uint16_t hold_ms_ = 400;

    uint16_t zero_ = 0;
    bool     zeroed_ = false;
    float    angle_ = 0.0f;
    float    error_ = 0.0f;
    bool     over_ = false;
    bool     slipping_ = false;
    uint32_t over_since_ = 0;
};
