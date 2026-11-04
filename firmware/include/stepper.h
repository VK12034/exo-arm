// Step generation for the TB6600.
//
// A timer interrupt ticks at cfg::kTickHz and accumulates a 16.16 fixed-point
// increment; each time the accumulator rolls past one, a step pulse goes out.
// Velocity therefore changes smoothly without re-arming a timer, and the
// interrupt does no floating-point work.
#pragma once

#include <Arduino.h>

#include "config.h"

class Stepper {
public:
    void begin(long position);

    // Sets the commanded velocity in steps per second, signed. Called from
    // the main loop only. A direction change is applied with the generator
    // stopped, so a reversal cannot emit a pulse against the old direction.
    void setVelocity(float steps_per_s);

    // Travel clamp enforced inside the interrupt, independent of the planner.
    // The planner should never reach these; if it does, this is what stops
    // the arm anyway.
    void setLimits(long min_steps, long max_steps);

    long position() const;
    void setPosition(long steps);

    void enable(bool on);
    bool enabled() const { return enabled_; }

    // Stops output and de-energises the driver in one call, for faults.
    void emergencyStop();

    // Called from the timer interrupt. Public only so the vector can reach it.
    void onTick();

    static Stepper* instance() { return instance_; }

private:
    void applyEnable();

    static Stepper* instance_;

    volatile uint32_t increment_ = 0;   // 16.16 steps per tick
    volatile uint32_t accumulator_ = 0;
    volatile long     position_ = 0;
    volatile int8_t   direction_ = 0;

    long min_steps_ = 0;
    long max_steps_ = 0;

    bool enabled_ = false;
};
