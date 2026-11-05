// Powered elbow firmware.
//
// The wearer holds UP or DOWN on the pendant and the elbow moves; everything
// else here exists to make sure it stops when it should. There is no encoder,
// so position is counted steps, and the counter only becomes trustworthy once
// the arm has been extended far enough to reach the home switch. Until then
// the arm runs slowly and on restricted travel. See docs/operation.md before
// running it.
#include <Arduino.h>
#include <EEPROM.h>
#include <avr/wdt.h>

#include "config.h"
#include "homing.h"
#include "journal.h"
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
Homing          g_homing;
Debounced       g_home_switch;
Journal         g_journal;

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

// Handed to the journal so its logic stays free of Arduino headers and the
// host tests can back it with a plain array.
uint8_t eepromRead(int address) { return EEPROM.read(address); }
void eepromWrite(int address, uint8_t value) { EEPROM.write(address, value); }

// The switch closes to ground, so a LOW reading is contact. Debounced like
// any other mechanical contact: a lever switch bounces, and a bounce here
// would re-zero the travel counter.
bool readHomeSwitchRaw() {
    const int level = digitalRead(cfg::kHomeSwitchPin);
    return cfg::kHomeSwitchClosesToGround ? (level == LOW) : (level == HIGH);
}

// Travel and speed both depend on whether zero has been established. An
// unhomed arm is allowed to seek below its assumed zero to find the switch,
// is held well short of full flexion, and runs at seeking speed so that
// whatever it is doing when the switch closes can be stopped in the gap
// between the switch and the hard stop.
void applyTravelLimits() {
    const bool  homed = g_homing.homed();
    const long  min_steps = homed ? cfg::kMinSteps : cfg::kSeekMinSteps;
    const long  max_steps = homed ? cfg::kMaxSteps : cfg::kUnhomedMaxSteps;
    const float speed     = homed ? cfg::kMaxSpeedSteps : cfg::kHomingSpeedSteps;

    g_stepper.setLimits(min_steps, max_steps);
    g_planner.configure(speed, cfg::kAccelSteps, min_steps, max_steps);
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

// Reported rather than silent, because the size of the correction is the only
// evidence the wearer gets about how much the counter drifts in a session.
// A few steps is rounding; a few degrees means the machine is losing them.
void reportZero() {
    Serial.print(F("homed: zero set at the extension switch"));

    if (g_homing.driftSignificant()) {
        Serial.print(F(", counter was out by "));
        Serial.print(g_homing.drift());
        Serial.print(F(" steps ("));
        Serial.print(angleDeg(g_homing.drift()), 2);
        Serial.print(F(" deg)"));
    }
    Serial.println();
}

// Printed once, at power-on, because there is no other way to read it and
// adding a serial command to ask would mean accepting serial input.
void printJournal(bool formatted) {
    Serial.print(F("log: session "));
    Serial.print(g_journal.sessions());

    if (formatted) {
        Serial.println(F(", initialised empty"));
        return;
    }

    const uint8_t held = g_journal.count();
    if (held == 0) {
        Serial.println(F(", nothing recorded"));
        return;
    }

    Serial.print(F(", "));
    Serial.print(held);
    Serial.println(F(" events, oldest first:"));

    Journal::Record record;
    for (uint8_t i = 0; i < held; ++i) {
        if (!g_journal.entry(i, record)) break;

        Serial.print(F("  s"));
        Serial.print(record.session);
        Serial.print(F(" "));

        switch (record.event) {
        case Journal::Event::kDrift:
            Serial.print(F("drift "));
            Serial.print(record.value);
            Serial.print(F(" steps ("));
            Serial.print(angleDeg(record.value), 2);
            Serial.println(F(" deg)"));
            break;

        case Journal::Event::kFaultOverload:
            Serial.print(F("overload at "));
            Serial.print(angleDeg(record.value), 1);
            Serial.println(F(" deg"));
            break;

        case Journal::Event::kFaultCollapse:
            Serial.print(F("current collapse at "));
            Serial.print(angleDeg(record.value), 1);
            Serial.println(F(" deg"));
            break;

        case Journal::Event::kFaultDrift:
            Serial.print(F("home switch disagreed, counter read "));
            Serial.print(angleDeg(record.value), 1);
            Serial.println(F(" deg"));
            break;

        case Journal::Event::kNone:
            Serial.println(F("(empty)"));
            break;
        }
    }
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
    Serial.print(F(" home="));
    Serial.print(g_home_switch.state() ? F("closed") : F("open"));
    Serial.print(F(" zero="));
    switch (g_homing.source()) {
    case Homing::Source::kSwitch:   Serial.print(F("switch")); break;
    case Homing::Source::kOperator: Serial.print(F("asserted")); break;
    case Homing::Source::kNone:     Serial.print(F("UNHOMED")); break;
    }
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

// The event code is what gets written to the log, and kNone means do not
// write one. Routine e-stop presses pass kNone: they are normal operation,
// the wearer knows they pressed it, and recording them would crowd the ring
// and spend EEPROM writes on something nobody needs told.
void enterFault(const __FlashStringHelper* reason, Journal::Event event) {
    g_stepper.emergencyStop();
    g_planner.stop();
    g_state = State::kFault;

    if (event != Journal::Event::kNone) {
        long position = g_stepper.position();
        if (position > 32767L) position = 32767L;
        if (position < -32768L) position = -32768L;
        g_journal.recordFault(event, static_cast<int16_t>(position));
    }

    Serial.print(F("FAULT: "));
    Serial.println(reason);
}

// A re-zero redefines where the travel limits sit, so it is only offered
// when the arm is stopped, and it is held for three seconds on purpose.
//
// This is the fallback for a build with no home switch fitted, and for
// recovering a session after a drift fault without power cycling. It asserts
// a zero rather than measuring one, so it is recorded as asserted and the
// status output says so. Reaching the switch is always better.
void handleRezero(uint32_t now_ms) {
    if (g_planner.moving()) return;

    if (g_pendant.bothHeldMs(now_ms) < cfg::kRezeroHoldMs) {
        if (g_pendant.bothHeldMs(now_ms) == 0) g_rezeroed = false;
        return;
    }

    if (g_rezeroed) return;

    g_stepper.setPosition(cfg::kMinSteps);
    g_homing.adoptOperatorZero();
    applyTravelLimits();
    g_rezeroed = true;

    Serial.println(F("rezeroed: this pose is asserted to be full extension"));
}

}  // namespace

void setup() {
    Serial.begin(115200);

    pinMode(cfg::kStatusLedPin, OUTPUT);
    pinMode(cfg::kOnboardLedPin, OUTPUT);

    // Opened before anything can fault, so a fault during start-up is still
    // recorded.
    const bool journal_formatted = g_journal.begin(eepromRead, eepromWrite, 0);

    const uint32_t now_ms = millis();

    g_pendant.begin(now_ms);

    // The counter starts at nominal zero, but nothing has measured that yet.
    // Parking the arm against its stop before switching on is still the
    // procedure, and now it is checked rather than assumed: the arm stays on
    // restricted travel until it reaches the home switch, and if it is parked
    // there already it homes on the first pass through the loop.
    g_stepper.begin(cfg::kMinSteps);

    pinMode(cfg::kHomeSwitchPin, INPUT_PULLUP);
    g_home_switch.configure(cfg::kDebounceMs);
    g_home_switch.reset(readHomeSwitchRaw(), now_ms);

    g_homing.configure(cfg::kMinSteps, cfg::kHomeToleranceSteps,
                       cfg::kHomeDriftFaultSteps);
    g_homing.reset();
    applyTravelLimits();

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
    Serial.print(F("unhomed: travel held to "));
    Serial.print(cfg::kUnhomedMaxAngleDeg, 0);
    Serial.print(F(" deg at "));
    Serial.print(cfg::kHomingSpeedDegPerSec, 0);
    Serial.println(F(" deg/s until the extension switch is reached"));
    printJournal(journal_formatted);

    wdt_enable(WDTO_1S);
}

void loop() {
    wdt_reset();

    const uint32_t now_ms = millis();
    const uint32_t now_us = micros();

    g_pendant.poll(now_ms);
    g_home_switch.update(readHomeSwitchRaw(), now_ms);

    // --- Interlocks --------------------------------------------------------
    if (g_pendant.eStopOpen()) {
        if (g_state != State::kFault) {
            enterFault(F("estop"), Journal::Event::kNone);
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
    long position = g_stepper.position();

    // --- Homing ------------------------------------------------------------
    // The switch is a physical feature of the machine and the counter is
    // arithmetic, so where they disagree the switch wins.
    switch (g_homing.update(g_home_switch.state(), position)) {
    case Homing::Action::kZero:
        g_stepper.setPosition(cfg::kMinSteps);
        applyTravelLimits();
        position = g_stepper.position();
        reportZero();

        // Only the corrections worth reporting are worth a write. A couple of
        // steps is rounding, and the log is for the trend.
        if (g_homing.driftSignificant()) {
            g_journal.recordDrift(static_cast<int16_t>(g_homing.drift()));
        }
        break;

    case Homing::Action::kFault:
        // Back to restricted travel before anything else happens, because
        // the far limit is no longer where it was believed to be either.
        applyTravelLimits();
        enterFault(F("home switch disagrees with the step counter - steps lost"),
                   Journal::Event::kFaultDrift);
        updateLeds(now_ms);
        return;

    case Homing::Action::kNone:
        break;
    }

    handleRezero(now_ms);

    g_planner.update(g_pendant.command(), position, dt);

    // Load is judged against whether the arm is being driven, not against
    // what the wearer asked for: a button held at a travel limit commands
    // nothing and should not be read as a stall.
    g_load.update(readAmps(), g_planner.moving(), now_ms);

    if (g_load.tripped()) {
        enterFault(g_load.overload() ? F("overload - the arm is pushing against something")
                                     : F("current collapse - possible stall or motor disconnected"),
                   g_load.overload() ? Journal::Event::kFaultOverload
                                     : Journal::Event::kFaultCollapse);
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
