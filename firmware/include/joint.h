// One actuated joint: encoder, motor driver, current sense and the control
// loop that ties them together.
#pragma once

#include <Arduino.h>
#include <Encoder.h>

#include "config.h"
#include "filter.h"
#include "pid.h"

class Joint {
public:
    explicit Joint(uint8_t index);

    // Configures pins and puts the driver to sleep. Call once from setup().
    void begin();

    // Zeroes the encoder at the current pose. The arm has no absolute
    // encoders, so the wearer parks it against the hard stop and this
    // defines that pose as the known reference.
    void zeroAt(float reference_rad);

    void setGains(float kp, float ki, float kd);

    // Position target, clamped to the soft limits.
    void setTarget(float rad);

    // Velocity target for assist mode, integrated into the position target
    // so that both modes drive the same inner loop.
    void setVelocityTarget(float rad_per_s);

    // Enabling resets the controller state so stale integral windup from
    // before the pause cannot kick the output.
    void enable();
    void disable();
    bool enabled() const { return enabled_; }

    // One control step. gravity_torque is the feed-forward term in Nm for
    // this joint at the current pose.
    void update(float dt, float gravity_torque);

    float position() const { return position_; }
    float velocity() const { return velocity_; }
    float target()   const { return target_; }
    float current()  const { return current_; }
    float duty()     const { return duty_; }
    float force()    const { return force_; }

    bool overCurrent()  const { return over_current_; }
    bool driverFault()  const;
    bool atSoftLimit()  const;

private:
    void   writeDuty(float duty);
    float  readCurrent();
    float  readForce();

    const uint8_t index_;
    Encoder       encoder_;
    Pid           pid_;
    LowPass       vel_filter_;
    LowPass       cur_filter_;
    LowPass       force_filter_;
    SlewLimiter   duty_slew_;

    int32_t count_offset_ = 0;
    int32_t prev_count_   = 0;

    float position_ = 0.0f;
    float velocity_ = 0.0f;
    float target_     = 0.0f;
    float vel_target_ = 0.0f;
    float current_  = 0.0f;
    float force_    = 0.0f;
    float duty_     = 0.0f;

    bool enabled_      = false;
    bool over_current_ = false;
};
