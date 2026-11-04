// Velocity planning for the elbow.
//
// The wearer holds a button; this turns that into a velocity that ramps
// rather than steps, and that comes to rest at the travel limits instead of
// arriving at them under power. It owns no hardware and includes no Arduino
// headers, so the native tests exercise it directly.
#pragma once

#include <stdint.h>

namespace motion {

// What the pendant is asking for.
enum class Command : int8_t {
    kExtend = -1,  // toward kMinSteps, arm straightening
    kHold   = 0,
    kFlex   = +1,  // toward kMaxSteps, arm bending
};

class Planner {
public:
    Planner() = default;

    Planner(float max_speed, float accel, long min_steps, long max_steps)
        : max_speed_(max_speed), accel_(accel),
          min_steps_(min_steps), max_steps_(max_steps) {}

    void configure(float max_speed, float accel, long min_steps, long max_steps) {
        max_speed_ = max_speed;
        accel_     = accel;
        min_steps_ = min_steps;
        max_steps_ = max_steps;
    }

    // Distance needed to come to rest from the given speed at this
    // deceleration. Deciding to stop any later than this means arriving at
    // the limit still moving.
    static float stoppingDistance(float speed, float accel) {
        if (accel <= 0.0f) return 0.0f;
        const float v = speed < 0.0f ? -speed : speed;
        return (v * v) / (2.0f * accel);
    }

    // One planner step. position is the current step count, dt the elapsed
    // time in seconds.
    void update(Command command, long position, float dt) {
        float desired = 0.0f;

        if (command == Command::kFlex && !blockedAbove(position)) {
            desired = max_speed_;
        } else if (command == Command::kExtend && !blockedBelow(position)) {
            desired = -max_speed_;
        }

        // Begin decelerating early enough to stop inside the limit, whatever
        // the wearer is asking for. Motion away from a limit is never damped.
        //
        // The margin carries one more tick's travel than the ideal stopping
        // distance. Deceleration happens in discrete steps, so the distance
        // actually covered while slowing is about half a tick's travel more
        // than the continuous figure, and the decision itself is taken a tick
        // before the motion it governs. Using the ideal distance alone
        // overshoots the limit by a few steps every time.
        //
        // One more step on top of that, because position arrives as a whole
        // step count. Travelling away from zero that count rounds down, so
        // the arm may be up to a step further along than it is reported to
        // be, and the planner has to assume it is.
        const float speed  = velocity_ < 0.0f ? -velocity_ : velocity_;
        const float margin = stoppingDistance(velocity_, accel_) + speed * dt + 1.0f;

        if (velocity_ > 0.0f && static_cast<float>(max_steps_ - position) <= margin) {
            desired = 0.0f;
        }
        if (velocity_ < 0.0f && static_cast<float>(position - min_steps_) <= margin) {
            desired = 0.0f;
        }

        const float previous = velocity_;
        const float step = accel_ * dt;

        if (desired > velocity_) {
            velocity_ += step;
            if (velocity_ > desired) velocity_ = desired;
        } else if (desired < velocity_) {
            velocity_ -= step;
            if (velocity_ < desired) velocity_ = desired;
        }

        // A reversal must put at least one update at exactly zero. The step
        // generator only moves the direction line while it is stopped, so a
        // velocity that jumps straight from positive to negative would have
        // it change direction with pulses still in flight. Nothing guarantees
        // the ramp lands on zero by itself.
        if ((previous > 0.0f && velocity_ < 0.0f) ||
            (previous < 0.0f && velocity_ > 0.0f)) {
            velocity_ = 0.0f;
        }

        // Below a step a second there is nothing useful left to emit, and
        // letting it tail off asymptotically would hold the driver awake.
        if (desired == 0.0f && velocity_ < 1.0f && velocity_ > -1.0f) {
            velocity_ = 0.0f;
        }
    }

    // Drops the velocity to zero without a ramp. For faults only: a stepper
    // commanded to stop dead may lose steps, which is why normal stops go
    // through update().
    void stop() { velocity_ = 0.0f; }

    float velocity() const { return velocity_; }

    bool moving() const { return velocity_ != 0.0f; }

    int8_t direction() const {
        if (velocity_ > 0.0f) return 1;
        if (velocity_ < 0.0f) return -1;
        return 0;
    }

    bool blockedAbove(long position) const { return position >= max_steps_; }
    bool blockedBelow(long position) const { return position <= min_steps_; }

    bool atLimit(long position) const {
        return blockedAbove(position) || blockedBelow(position);
    }

private:
    float max_speed_ = 0.0f;
    float accel_     = 0.0f;
    long  min_steps_ = 0;
    long  max_steps_ = 0;

    float velocity_ = 0.0f;
};

// Converts a signed step velocity into the per-tick increment used by the
// fixed-point step generator, in 16.16 format. Keeping this here lets the
// tests pin the arithmetic that the interrupt depends on.
inline uint32_t tickIncrement(float velocity_steps_per_s, uint16_t tick_hz) {
    float v = velocity_steps_per_s < 0.0f ? -velocity_steps_per_s : velocity_steps_per_s;
    if (tick_hz == 0) return 0;

    // One step per tick is the ceiling: asking for more would silently drop
    // steps, so the position counter would stop matching the real arm.
    const float max_v = static_cast<float>(tick_hz);
    if (v > max_v) v = max_v;

    return static_cast<uint32_t>((v * 65536.0f) / static_cast<float>(tick_hz));
}

}  // namespace motion
