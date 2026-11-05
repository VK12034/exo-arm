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

// Hall-effect current sensor in the 24 V feed to the driver.
constexpr uint8_t kCurrentSensePin = A0;

// Lever switch at the extension end of travel, used to find zero. Normally
// open, closing to ground when the arm reaches full extension, read with the
// internal pull-up. That polarity is chosen for how it fails: a broken wire
// or an unplugged switch reads open forever, so the arm simply never homes
// and stays on its restricted travel. Wired the other way round, the same
// break would read as "at home" and invent a zero wherever the arm happened
// to be.
constexpr uint8_t kHomeSwitchPin = 9;
constexpr bool kHomeSwitchClosesToGround = true;

// Lever switch sited just past the flexion soft limit. Same polarity and the
// same reasoning: a break reads open, which costs a check rather than
// inventing one. This switch should never be reached in normal use, so unlike
// the home switch a broken one is invisible until it is needed. Step 6 of
// docs/commissioning.md is the only thing that proves it works.
constexpr uint8_t kFlexSwitchPin = 10;
constexpr bool kFlexSwitchClosesToGround = true;

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

// --- Homing ----------------------------------------------------------------
// Until the home switch has been reached, the step counter is an assumption
// about how the arm was parked, so flexion is held well inside the full range.
// Reaching the switch lifts this. 90 degrees is enough to be useful and not
// enough for a 45 degree parking error to put the elbow somewhere it should
// not be.
constexpr float kUnhomedMaxAngleDeg = 90.0f;
constexpr long  kUnhomedMaxSteps =
    static_cast<long>(kUnhomedMaxAngleDeg * kStepsPerDegree);

// An unhomed arm has to be allowed to travel below its assumed zero, or it
// could never reach the switch that defines zero. The real floor while
// seeking is the switch, and the hard stop behind it. A full range of
// allowance covers any plausible parking error.
constexpr long kSeekMinSteps = -kMaxSteps;

// Seeking speed. The switch can only be acted on once it is reached, so
// whatever the arm is doing at that moment is overshoot, and it has to fit
// between the switch and the hard stop. Note that neither the stop nor the
// switch bracket is in the CAD yet: docs/safety.md lists that as the largest
// gap on the page, and until they exist this figure is sizing a clearance
// nothing provides. At 5 deg/s this is 0.28 degrees of
// stopping distance against 45 deg/s^2, so the switch wants siting at least a
// degree clear of the stop. docs/commissioning.md gives the measurement.
constexpr float kHomingSpeedDegPerSec = 5.0f;
constexpr float kHomingSpeedSteps = kHomingSpeedDegPerSec * kStepsPerDegree;

// How far the counter may disagree with the switch before it is worth saying
// so. Position arrives as a whole step count and the switch has hysteresis of
// its own, so small disagreements are expected and corrected silently.
constexpr long kHomeToleranceSteps = static_cast<long>(0.5f * kStepsPerDegree);

// Past this, enough steps have been lost that the far limit is no longer
// where it is believed to be. Re-zeroing would hide that, so it faults.
constexpr long kHomeDriftFaultSteps = static_cast<long>(5.0f * kStepsPerDegree);

// --- Overrun switch --------------------------------------------------------
// Where the flexion switch sits, past kMaxAngleDeg. Reaching it means the
// planner and the step generator's own clamp both failed to stop at the soft
// limit, so it faults rather than merely stopping.
//
// Far enough past the limit that a normal stop does not brush it: the planner
// comes to rest at 135 degrees and the clamp refuses to pulse beyond it, so
// 3 degrees is several times any plausible overshoot.
constexpr float kFlexSwitchAngleDeg = 138.0f;
constexpr long  kFlexSwitchSteps =
    static_cast<long>(kFlexSwitchAngleDeg * kStepsPerDegree);

// The switch is at a fixed place on the machine, so the step count when it
// closes measures the home-to-flexion span. Compared against
// kFlexSwitchSteps, that is the only check available on whether the TB6600's
// microstep DIP switches agree with kMicrosteps: disagree and the span is out
// by the ratio between them, which no tolerance would absorb.
//
// Two degrees allows for switch hysteresis, lever flex and the usual rounding
// without absorbing anything that matters.
constexpr long kSpanToleranceSteps = static_cast<long>(2.0f * kStepsPerDegree);

// A closure below this says the switch is shorted or on the wrong pin rather
// than that the arm is at the flexion end. Half the span is well clear of any
// plausible siting error.
constexpr long kFlexMinPlausibleSteps = kFlexSwitchSteps / 2;

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

// --- Load monitoring -------------------------------------------------------
// ACS712-20A in the driver's 24 V feed: 100 mV/A about a mid-rail zero, read
// on a 5 V 10-bit ADC. That is about 49 mA per count, which is coarse but far
// finer than the thresholds below.
constexpr float kAmpsPerCount = (5.0f / 1024.0f) / 0.100f;

// The zero point is measured at boot with the driver de-energised rather
// than assumed to be mid-rail, because these sensors drift with temperature
// and supply.
constexpr uint8_t kCurrentZeroSamples = 64;

// Every threshold here is a placeholder until it is measured on the bench.
// docs/commissioning.md step 10 says to record the real figures; these are
// sized for a NEMA 23 at roughly 2 A per phase off a 24 V supply.
constexpr float    kOverloadAmps  = 2.5f;   // sustained draw that means binding
constexpr uint16_t kOverloadMs    = 400;
constexpr float    kCollapseAmps  = 0.15f;  // draw that means no useful work
constexpr uint16_t kCollapseMs    = 600;

// Current needs time to settle after motion starts, and the first step draws
// an inrush that would otherwise read as an overload.
constexpr uint16_t kLoadGraceMs = 250;

// One-pole filter on the current reading. A stepper driver chops
// continuously, so the raw reading is noisy at the sample rate.
constexpr float kCurrentFilterAlpha = 0.05f;

}  // namespace cfg
