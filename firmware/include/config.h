// Build-time configuration for the powered elbow.
//
// Mechanical figures come from the Fusion model via cad/BudgetExoArm.design.json.
// Pin assignments are a wiring decision, not something the CAD fixes; they are
// mirrored in docs/wiring.md and the two must be changed together.
#pragma once

#include <stdint.h>

namespace cfg {

// --- Pin map (Arduino Uno) -------------------------------------------------
// The TB6600 opto inputs are driven in common-cathode wiring: PUL+, DIR+ and
// ENA+ to 5 V, and these pins pulled low to activate.
constexpr uint8_t kStepPin   = 3;
constexpr uint8_t kDirPin    = 4;
constexpr uint8_t kEnablePin = 5;

// E-stop sits on pin 2 so it can also raise an external interrupt if the loop
// is ever extended to need one.
constexpr uint8_t kEStopPin = 2;
constexpr uint8_t kUpPin    = 6;
constexpr uint8_t kDownPin  = 7;

constexpr uint8_t kStatusLedPin = 8;   // green LED on the control box face
constexpr uint8_t kOnboardLedPin = 13;

// The TB6600 enable input is active low in this wiring: pulling ENA low
// energises the motor.
constexpr bool kEnableActiveLow = true;

// --- Drivetrain ------------------------------------------------------------
// NEMA 23 (23HS30) through an NMRV030 worm, 15:1, driving the elbow directly.
constexpr uint16_t kMotorStepsPerRev = 200;
constexpr uint8_t  kMicrosteps       = 8;     // TB6600 DIP switches
constexpr uint8_t  kGearRatio        = 15;

constexpr float kStepsPerOutputRev =
    static_cast<float>(kMotorStepsPerRev) * kMicrosteps * kGearRatio;  // 24000
constexpr float kStepsPerDegree = kStepsPerOutputRev / 360.0f;         // 66.667

// Set true if the elbow flexes when DIR is low rather than high. Verify on the
// bench with the arm unworn before anyone puts it on: see docs/commissioning.md.
constexpr bool kInvertDirection = false;

// --- Travel ----------------------------------------------------------------
// Zero is full extension, the pose the arm is parked in at power-on. Positive
// is flexion. The upper limit is set inside the wearer's comfortable range,
// not at the mechanism's hard stop.
constexpr float kMinAngleDeg = 0.0f;
constexpr float kMaxAngleDeg = 135.0f;

constexpr long kMinSteps = static_cast<long>(kMinAngleDeg * kStepsPerDegree);
constexpr long kMaxSteps = static_cast<long>(kMaxAngleDeg * kStepsPerDegree);

// --- Motion profile --------------------------------------------------------
// Deliberately slow. This is a limb, and the worm means a command error cannot
// be corrected by the wearer pushing back.
constexpr float kMaxSpeedDegPerSec = 20.0f;
constexpr float kAccelDegPerSec2   = 45.0f;

constexpr float kMaxSpeedSteps = kMaxSpeedDegPerSec * kStepsPerDegree;
constexpr float kAccelSteps    = kAccelDegPerSec2 * kStepsPerDegree;

// --- Timing ----------------------------------------------------------------
constexpr uint16_t kTickHz        = 10000;  // step-generator interrupt rate
constexpr uint16_t kPlannerHz     = 1000;   // velocity updates in the main loop
constexpr uint16_t kDebounceMs    = 25;
constexpr uint16_t kStatusPeriodMs = 500;

// The worm is self-locking, so the elbow holds its angle with the motor
// de-energised. Dropping the current after a pause keeps the motor and driver
// cool and saves the wearer carrying the noise and heat of a holding current.
constexpr uint16_t kIdleDisableMs = 3000;

// Holding both buttons for this long re-zeroes the travel counter at the
// current pose. Needed because step loss has nothing to correct it, but it
// also defeats the soft limits, so it is deliberately awkward to trigger.
constexpr uint16_t kRezeroHoldMs = 3000;

// TB6600 wants at least 2.2 us of pulse width; 4 us gives margin without
// meaningfully lengthening the interrupt.
constexpr uint8_t kStepPulseUs = 4;

}  // namespace cfg
