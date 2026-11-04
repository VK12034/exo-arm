// Build-time configuration: pin map, joint limits, control rates.
// Everything a new build of the arm needs to change should live here.
#pragma once

#include <stdint.h>

namespace cfg {

// --- Control rates ---------------------------------------------------------
constexpr uint32_t kControlHz   = 1000;
constexpr uint32_t kTelemetryHz = 100;
constexpr float    kDt          = 1.0f / static_cast<float>(kControlHz);

// --- Joint count -----------------------------------------------------------
// J0 = shoulder flexion/extension, J1 = elbow flexion/extension.
constexpr uint8_t kJointCount = 2;

// --- Motor driver pins (Pololu G2 High-Power, one per joint) ---------------
// PWM drives magnitude, DIR sets direction, SLP is active-low sleep,
// FLT is the driver's open-drain fault output, CS is analog current sense.
struct DriverPins {
    uint8_t pwm;
    uint8_t dir;
    uint8_t slp;
    uint8_t flt;
    uint8_t cs;
};

constexpr DriverPins kDriver[kJointCount] = {
    {  2,  3,  4,  5, A0 },  // J0 shoulder
    {  6,  7,  8,  9, A1 },  // J1 elbow
};

// --- Quadrature encoder pins (both must be interrupt-capable on Teensy) ----
struct EncoderPins {
    uint8_t a;
    uint8_t b;
};

constexpr EncoderPins kEncoder[kJointCount] = {
    { 14, 15 },  // J0
    { 16, 17 },  // J1
};

// --- Discrete safety I/O ---------------------------------------------------
constexpr uint8_t kEStopPin    = 20;  // NC loop to GND: open == stop
constexpr uint8_t kDeadmanPin  = 21;  // operator grip switch, active low
constexpr uint8_t kStatusLed   = 13;

// --- Interaction force sensing (load cell amp, one per joint) --------------
constexpr uint8_t kForceSensePin[kJointCount] = { A2, A3 };

// --- Mechanics -------------------------------------------------------------
// Counts per output revolution = encoder CPR * gear ratio * quadrature.
constexpr float kEncoderCpr[kJointCount]  = { 2048.0f, 2048.0f };
constexpr float kGearRatio[kJointCount]   = {   99.5f,   71.2f };
constexpr float kCountsPerRad[kJointCount] = {
    kEncoderCpr[0] * kGearRatio[0] * 4.0f / 6.28318530718f,
    kEncoderCpr[1] * kGearRatio[1] * 4.0f / 6.28318530718f,
};

// Link lengths, metres. Shoulder joint to elbow joint, elbow to wrist cuff.
constexpr float kLinkLength[kJointCount] = { 0.280f, 0.255f };

// Mass of each link plus its share of the cuff and actuator, kg, and the
// distance from the proximal joint to that link's centre of mass, metres.
constexpr float kLinkMass[kJointCount]   = { 0.84f, 0.52f };
constexpr float kLinkCom[kJointCount]    = { 0.145f, 0.120f };

// --- Soft joint limits, radians --------------------------------------------
// Set inside the mechanical hard stops with margin for overshoot.
constexpr float kMinAngle[kJointCount] = { -0.35f,  0.00f };
constexpr float kMaxAngle[kJointCount] = {  2.60f,  2.35f };

// Travel limit enforcement starts this far from the limit, so the joint is
// ramped to a stop rather than hitting the clamp at speed.
constexpr float kLimitMarginRad = 0.12f;

// --- Actuation limits ------------------------------------------------------
constexpr float kMaxVelocity[kJointCount] = { 2.0f, 2.6f };   // rad/s
constexpr float kMaxCurrent[kJointCount]  = { 6.0f, 4.5f };   // A, per joint
constexpr float kMaxDutySlew              = 4.0f;             // 1/s, full scale
constexpr float kTorqueConstant[kJointCount] = { 0.0214f, 0.0185f }; // Nm/A

// Current sense scaling for the G2 driver: 20 mV/A into a 3.3 V, 10-bit ADC.
constexpr float kCurrentSenseAmpsPerCount = (3.3f / 1023.0f) / 0.020f;

// --- Default gains ---------------------------------------------------------
// Tuned per docs/tuning.md; overridable at runtime over the serial link.
struct Gains {
    float kp;
    float ki;
    float kd;
};

// Gravity feed-forward is applied as a duty term rather than through a full
// motor model. The scale is measured per joint during commissioning: see
// docs/tuning.md, "Measuring the gravity feed-forward scale".
constexpr float kGravityFfDutyPerNm[kJointCount] = { 0.072f, 0.095f };

// Velocity is differentiated from encoder counts, so it needs filtering
// before it is usable as a derivative term or a telemetry value.
constexpr float kVelocityCutoffHz = 40.0f;
constexpr float kCurrentCutoffHz  = 60.0f;
constexpr float kForceCutoffHz    = 12.0f;

// Force sensor scaling: amplifier output is centred at mid-rail and swings
// symmetrically, so counts are offset then scaled to newtons.
constexpr float kForceZeroCounts     = 511.5f;
constexpr float kForceNewtonsPerCount = 0.185f;

constexpr Gains kDefaultGains[kJointCount] = {
    { 9.0f, 1.8f, 0.22f },  // J0
    { 7.5f, 1.4f, 0.16f },  // J1
};

// --- Assist behaviour ------------------------------------------------------
// Admittance mode converts measured interaction force into a velocity
// command: v = gain * force, clamped to kMaxVelocity.
constexpr float kDefaultAssistGain = 0.04f;   // (rad/s) per newton
constexpr float kForceDeadbandN    = 1.5f;

// --- Watchdog --------------------------------------------------------------
// Host link must be seen at least this often while in an active mode.
constexpr uint32_t kHostTimeoutMs = 500;
}
