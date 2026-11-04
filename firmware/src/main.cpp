// Powered elbow firmware.
//
// The wearer holds UP or DOWN on the pendant and the elbow moves; everything
// else here exists to make sure it stops when it should. There is no encoder,
// so position is counted steps from the pose the arm was parked in at
// power-on. See docs/operation.md before running it.
#include <Arduino.h>
#include <avr/wdt.h>

#include "config.h"
#include "load.h"
#include "motion.h"
#include "pendant.h"
#include "stepper.h"

namespace {

enum class State : uint8_t {
    kFault,   // e-stop open, a load trip, or waiting for the buttons to be let go
    kIdle,
    kMoving,
};

Stepper         g_stepper;
Pendant         g_pendant;
motion::Planner g_planner;
LoadMonitor     g_load;

State    g_state = State::kFault;
uint32_t g_last_plan_us = 0;
uint32_t g_last_status_ms = 0;
uint32_t g_idle_since_ms = 0;
bool     g_rezeroed = false;

// ADC counts read with the driver de-energised, so the sensor's own offset
// is measured rather than assumed.
int g_current_zero = 512;

float readAmps() {
    const int counts = analogRead(cfg::kCurrentSensePin) - g_current_zero;
    const float amps = static_cast<float>(counts) * cfg::kAmpsPerCount;
    return amps < 0.0f ? -amps : amps;
}

void setLeds(bool status, bool onboard) {
    digitalWrite(cfg::kStatusLedPin, status ? HIGH : LOW);
    digitalWrite(cfg::kOnboardLedPin, onboard ? HIGH : LOW);
}

// Solid while driving, slow blink when ready, fast blink on a fault.
void updateLeds(uint32_t now_ms) {
    switch (g_state) {
    case State::kMoving:
        setLeds(true, true);
        break;
    case State::kIdle: {
        const bool on = (now_ms / 800) % 2 == 0;
        setLeds(on, on);
        break;
    }
    case State::kFault: {
        const bool on = (now_ms / 120) % 2 == 0;
        setLeds(on, on);
        break;
    }
    }
}

float angleDeg(long steps) {
    return static_cast<float>(steps) / cfg::kStepsPerDegree;
}

void printStatus(uint32_t now_ms, long position) {
    const char* state =
        g_state == State::kFault ? "FAULT" : (g_state == State::kMoving ? "MOVING" : "IDLE");

    Serial.print(F("t="));
    Serial.print(now_ms);
    Serial.print(F(" state="));
    Serial.print(state);
    Serial.print(F(" angle="));
    Serial.print(angleDeg(position), 1);
    Serial.print(F("deg steps="));
    Serial.print(position);
    Serial.print(F(" v="));
    Serial.print(g_planner.velocity(), 0);
    Serial.print(F(" estop="));
    Serial.print(g_pendant.eStopOpen() ? F("OPEN") : F("closed"));
    Serial.print(F(" amps="));
    Serial.print(g_load.current(), 2);
    Serial.print(F(" drive="));
    Serial.println(g_stepper.enabled() ? F("on") : F("off"));
}

// Measures the current sensor's zero with the driver de-energised. Anything
// moving during this would be read as the zero point for the rest of the
// session, so it runs before the motor can be enabled.
void calibrateCurrentZero() {
    long total = 0;
    for (uint8_t i = 0; i < cfg::kCurrentZeroSamples; ++i) {
        total += analogRead(cfg::kCurrentSensePin);
        delay(2);
    }
    g_current_zero = static_cast<int>(total / cfg::kCurrentZeroSamples);

    Serial.print(F("current zero: "));
    Serial.print(g_current_zero);
    Serial.println(F(" counts"));
}

void enterFault(const __FlashStringHelper* reason) {
    g_stepper.emergencyStop();
    g_planner.stop();
    g_state = State::kFault;

    Serial.print(F("FAULT: "));
    Serial.println(reason);
}

// A re-zero redefines where the travel limits sit, so it is only offered
// when the arm is stopped, and it is held for three seconds on purpose.
void handleRezero(uint32_t now_ms) {
    if (g_planner.moving()) return;

    if (g_pendant.bothHeldMs(now_ms) < cfg::kRezeroHoldMs) {
        if (g_pendant.bothHeldMs(now_ms) == 0) g_rezeroed = false;
        return;
    }

    if (g_rezeroed) return;

    g_stepper.setPosition(cfg::kMinSteps);
    g_rezeroed = true;

    Serial.println(F("rezeroed: this pose is now full extension"));
}

}  // namespace

void setup() {
    Serial.begin(115200);

    pinMode(cfg::kStatusLedPin, OUTPUT);
    pinMode(cfg::kOnboardLedPin, OUTPUT);

    const uint32_t now_ms = millis();

    g_pendant.begin(now_ms);

    // Power-on pose is taken to be full extension. Nothing measures this, so
    // parking the arm against its stop before switching on is part of the
    // operating procedure, not a nicety.
    g_stepper.begin(cfg::kMinSteps);
    g_stepper.setLimits(cfg::kMinSteps, cfg::kMaxSteps);

    g_planner.configure(cfg::kMaxSpeedSteps, cfg::kAccelSteps, cfg::kMinSteps, cfg::kMaxSteps);

    calibrateCurrentZero();
    g_load.configure(cfg::kOverloadAmps, cfg::kOverloadMs,
                     cfg::kCollapseAmps, cfg::kCollapseMs,
                     cfg::kLoadGraceMs, cfg::kCurrentFilterAlpha);
    g_load.reset(readAmps());

    g_last_plan_us = micros();
    g_idle_since_ms = now_ms;

    // Start in fault until the e-stop is seen closed and both buttons are up,
    // so a stuck button cannot drive the arm the moment power arrives.
    g_state = State::kFault;

    Serial.println(F("exo-arm elbow ready"));
    Serial.print(F("travel 0-"));
    Serial.print(cfg::kMaxAngleDeg, 0);
    Serial.print(F(" deg, "));
    Serial.print(cfg::kStepsPerDegree, 1);
    Serial.println(F(" steps/deg"));

    wdt_enable(WDTO_1S);
}

void loop() {
    wdt_reset();

    const uint32_t now_ms = millis();
    const uint32_t now_us = micros();

    g_pendant.poll(now_ms);

    // --- Interlocks --------------------------------------------------------
    if (g_pendant.eStopOpen()) {
        if (g_state != State::kFault) {
            enterFault(F("estop"));
        }
        updateLeds(now_ms);
        return;
    }

    if (g_state == State::kFault) {
        // The interlock has cleared. Wait for both buttons to be released
        // before handing control back, so releasing the mushroom while a
        // button is held does not start the arm moving.
        if (!g_pendant.upHeld() && !g_pendant.downHeld()) {
            g_load.clear();
            g_state = State::kIdle;
            g_idle_since_ms = now_ms;
            g_last_plan_us = now_us;
            Serial.println(F("ready"));
        }
        updateLeds(now_ms);
        return;
    }

    // --- Planner -----------------------------------------------------------
    const uint32_t elapsed_us = now_us - g_last_plan_us;
    if (elapsed_us < (1000000UL / cfg::kPlannerHz)) {
        updateLeds(now_ms);
        return;
    }
    g_last_plan_us = now_us;

    const float dt = static_cast<float>(elapsed_us) * 1e-6f;
    const long position = g_stepper.position();

    handleRezero(now_ms);

    g_planner.update(g_pendant.command(), position, dt);

    // Load is judged against whether the arm is being driven, not against
    // what the wearer asked for: a button held at a travel limit commands
    // nothing and should not be read as a stall.
    g_load.update(readAmps(), g_planner.moving(), now_ms);

    if (g_load.tripped()) {
        enterFault(g_load.overload() ? F("overload - the arm is pushing against something")
                                     : F("current collapse - possible stall or motor disconnected"));
        updateLeds(now_ms);
        return;
    }

    if (g_planner.moving()) {
        g_stepper.enable(true);
        g_stepper.setVelocity(g_planner.velocity());
        g_state = State::kMoving;
        g_idle_since_ms = now_ms;
    } else {
        g_stepper.setVelocity(0.0f);
        g_state = State::kIdle;

        // The worm holds the elbow without current, so there is nothing to be
        // gained from keeping the motor energised once it has settled.
        if (g_stepper.enabled() && (now_ms - g_idle_since_ms) > cfg::kIdleDisableMs) {
            g_stepper.enable(false);
        }
    }

    if ((now_ms - g_last_status_ms) >= cfg::kStatusPeriodMs) {
        g_last_status_ms = now_ms;
        printStatus(now_ms, position);
    }

    updateLeds(now_ms);
}
