#include "joint.h"

namespace {

float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

}  // namespace

Joint::Joint(uint8_t index)
    : index_(index),
      encoder_(cfg::kEncoder[index].a, cfg::kEncoder[index].b),
      pid_(cfg::kDefaultGains[index].kp,
           cfg::kDefaultGains[index].ki,
           cfg::kDefaultGains[index].kd,
           1.0f),
      vel_filter_(cfg::kVelocityCutoffHz, cfg::kDt),
      cur_filter_(cfg::kCurrentCutoffHz, cfg::kDt),
      force_filter_(cfg::kForceCutoffHz, cfg::kDt),
      duty_slew_(cfg::kMaxDutySlew) {}

void Joint::begin() {
    const cfg::DriverPins& d = cfg::kDriver[index_];

    pinMode(d.pwm, OUTPUT);
    pinMode(d.dir, OUTPUT);
    pinMode(d.slp, OUTPUT);
    pinMode(d.flt, INPUT_PULLUP);

    // Teensy 4.x defaults to 4.4 kHz on most pins, which is audible and hard
    // on the driver. 20 kHz at 12-bit keeps the motor quiet and the current
    // ripple low.
    analogWriteFrequency(d.pwm, 20000);
    analogWriteResolution(12);
    analogReadResolution(10);

    digitalWrite(d.slp, LOW);  // driver asleep until the joint is enabled
    analogWrite(d.pwm, 0);

    prev_count_ = encoder_.read();
}

void Joint::zeroAt(float reference_rad) {
    const int32_t raw = encoder_.read();
    count_offset_ = raw - static_cast<int32_t>(reference_rad * cfg::kCountsPerRad[index_]);
    prev_count_   = raw;

    position_   = reference_rad;
    target_     = reference_rad;
    vel_target_ = 0.0f;
    velocity_   = 0.0f;

    vel_filter_.reset(0.0f);
    pid_.reset(reference_rad);
}

void Joint::setGains(float kp, float ki, float kd) {
    pid_.setGains(kp, ki, kd);
}

void Joint::setTarget(float rad) {
    target_ = clampf(rad, cfg::kMinAngle[index_], cfg::kMaxAngle[index_]);
}

void Joint::setVelocityTarget(float rad_per_s) {
    vel_target_ = clampf(rad_per_s, -cfg::kMaxVelocity[index_], cfg::kMaxVelocity[index_]);
}

void Joint::enable() {
    if (enabled_) return;

    // Start from where the joint actually is, holding that pose, so enabling
    // never produces a step command.
    target_     = position_;
    vel_target_ = 0.0f;

    pid_.reset(position_);
    duty_slew_.reset(0.0f);

    digitalWrite(cfg::kDriver[index_].slp, HIGH);
    enabled_ = true;
}

void Joint::disable() {
    enabled_ = false;
    vel_target_ = 0.0f;
    duty_slew_.reset(0.0f);
    writeDuty(0.0f);
    digitalWrite(cfg::kDriver[index_].slp, LOW);
}

bool Joint::driverFault() const {
    // The G2 drives FLT low on fault; the pin is otherwise held high by the
    // internal pull-up.
    return digitalRead(cfg::kDriver[index_].flt) == LOW;
}

bool Joint::atSoftLimit() const {
    return position_ < cfg::kMinAngle[index_] || position_ > cfg::kMaxAngle[index_];
}

float Joint::readCurrent() {
    const int raw = analogRead(cfg::kDriver[index_].cs);
    return cur_filter_.update(static_cast<float>(raw) * cfg::kCurrentSenseAmpsPerCount);
}

float Joint::readForce() {
    const int raw = analogRead(cfg::kForceSensePin[index_]);
    const float newtons =
        (static_cast<float>(raw) - cfg::kForceZeroCounts) * cfg::kForceNewtonsPerCount;
    return force_filter_.update(newtons);
}

void Joint::update(float dt, float gravity_torque) {
    // --- Sense ------------------------------------------------------------
    const int32_t raw = encoder_.read();
    position_ = static_cast<float>(raw - count_offset_) / cfg::kCountsPerRad[index_];

    const float raw_vel =
        static_cast<float>(raw - prev_count_) / cfg::kCountsPerRad[index_] / dt;
    prev_count_ = raw;
    velocity_   = vel_filter_.update(raw_vel);

    current_ = readCurrent();
    force_   = readForce();

    over_current_ = current_ > cfg::kMaxCurrent[index_];

    if (!enabled_) {
        duty_ = 0.0f;
        writeDuty(0.0f);
        return;
    }

    // --- Target -----------------------------------------------------------
    if (vel_target_ != 0.0f) {
        setTarget(target_ + vel_target_ * dt);
    }

    // --- Control ----------------------------------------------------------
    float command = pid_.update(target_, position_, dt);

    command += gravity_torque * cfg::kGravityFfDutyPerNm[index_];

    // Ease off as the joint nears a soft limit so it decelerates into the
    // stop instead of arriving at speed. Motion away from the limit is left
    // untouched so the wearer can always drive back out.
    const float to_min = position_ - cfg::kMinAngle[index_];
    const float to_max = cfg::kMaxAngle[index_] - position_;

    if (to_max < cfg::kLimitMarginRad && command > 0.0f) {
        command *= clampf(to_max / cfg::kLimitMarginRad, 0.0f, 1.0f);
    }
    if (to_min < cfg::kLimitMarginRad && command < 0.0f) {
        command *= clampf(to_min / cfg::kLimitMarginRad, 0.0f, 1.0f);
    }

    // Velocity ceiling: refuse to add command in the direction the joint is
    // already moving too fast.
    if (velocity_ > cfg::kMaxVelocity[index_] && command > 0.0f) command = 0.0f;
    if (velocity_ < -cfg::kMaxVelocity[index_] && command < 0.0f) command = 0.0f;

    // Current ceiling, applied as a proportional cutback rather than a hard
    // trip so that brief peaks do not stop the arm dead.
    if (current_ > cfg::kMaxCurrent[index_] * 0.8f) {
        const float headroom = clampf(
            (cfg::kMaxCurrent[index_] - current_) / (cfg::kMaxCurrent[index_] * 0.2f),
            0.0f, 1.0f);
        command *= headroom;
    }

    duty_ = duty_slew_.update(clampf(command, -1.0f, 1.0f), dt);
    writeDuty(duty_);
}

void Joint::writeDuty(float duty) {
    const cfg::DriverPins& d = cfg::kDriver[index_];

    digitalWrite(d.dir, duty >= 0.0f ? HIGH : LOW);

    const float magnitude = clampf(fabsf(duty), 0.0f, 1.0f);
    analogWrite(d.pwm, static_cast<int>(magnitude * 4095.0f));
}
