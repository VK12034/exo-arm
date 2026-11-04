// Position PID with derivative-on-measurement, conditional integration and
// output clamping. Header-only and free of Arduino dependencies so the
// native unit tests can exercise it directly.
#pragma once

class Pid {
public:
    Pid() = default;

    Pid(float kp, float ki, float kd, float out_limit)
        : kp_(kp), ki_(ki), kd_(kd), out_limit_(out_limit) {}

    void setGains(float kp, float ki, float kd) {
        kp_ = kp;
        ki_ = ki;
        kd_ = kd;
    }

    void setOutputLimit(float limit) { out_limit_ = limit; }

    // Derivative is taken on the measurement rather than the error so that a
    // step change in setpoint does not produce a derivative spike.
    float update(float setpoint, float measurement, float dt) {
        const float error = setpoint - measurement;

        const float d_meas = (dt > 0.0f) ? (measurement - prev_meas_) / dt : 0.0f;
        prev_meas_ = measurement;

        const float unclamped = kp_ * error + ki_ * (integral_ + error * dt) - kd_ * d_meas;

        // Conditional integration: only accumulate when doing so does not
        // drive the output further past the limit it is already against.
        const bool saturated_high = unclamped > out_limit_ && error > 0.0f;
        const bool saturated_low  = unclamped < -out_limit_ && error < 0.0f;
        if (!saturated_high && !saturated_low) {
            integral_ += error * dt;
        }

        const float out = kp_ * error + ki_ * integral_ - kd_ * d_meas;
        return clamp(out, -out_limit_, out_limit_);
    }

    // Call when a joint is re-enabled so stale integral and derivative state
    // from before the pause cannot kick the output.
    void reset(float measurement = 0.0f) {
        integral_   = 0.0f;
        prev_meas_  = measurement;
    }

    float integral() const { return integral_; }

    static float clamp(float v, float lo, float hi) {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }

private:
    float kp_ = 0.0f;
    float ki_ = 0.0f;
    float kd_ = 0.0f;
    float out_limit_ = 1.0f;

    float integral_  = 0.0f;
    float prev_meas_ = 0.0f;
};
