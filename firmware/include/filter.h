// Small signal-conditioning helpers shared by the joint controllers.
#pragma once

// First-order IIR low-pass. cutoff_hz of 0 disables filtering.
class LowPass {
public:
    LowPass() = default;
    LowPass(float cutoff_hz, float dt) { configure(cutoff_hz, dt); }

    void configure(float cutoff_hz, float dt) {
        if (cutoff_hz <= 0.0f || dt <= 0.0f) {
            alpha_ = 1.0f;
            return;
        }
        const float rc = 1.0f / (6.28318530718f * cutoff_hz);
        alpha_ = dt / (rc + dt);
    }

    float update(float x) {
        y_ += alpha_ * (x - y_);
        return y_;
    }

    void reset(float value = 0.0f) { y_ = value; }
    float value() const { return y_; }

private:
    float alpha_ = 1.0f;
    float y_     = 0.0f;
};

// Limits how fast a command may change, in units per second. Used on motor
// duty so a sudden setpoint change cannot step the current.
class SlewLimiter {
public:
    SlewLimiter() = default;
    explicit SlewLimiter(float rate_per_s) : rate_(rate_per_s) {}

    float update(float target, float dt) {
        const float max_step = rate_ * dt;
        const float delta    = target - y_;
        if (delta > max_step)       y_ += max_step;
        else if (delta < -max_step) y_ -= max_step;
        else                        y_ = target;
        return y_;
    }

    void reset(float value = 0.0f) { y_ = value; }
    float value() const { return y_; }

private:
    float rate_ = 1.0f;
    float y_    = 0.0f;
};
