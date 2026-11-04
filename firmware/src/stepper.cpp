#include "stepper.h"

#include "motion.h"

Stepper* Stepper::instance_ = nullptr;

void Stepper::begin(long position) {
    instance_ = this;

    pinMode(cfg::kStepPin, OUTPUT);
    pinMode(cfg::kDirPin, OUTPUT);
    pinMode(cfg::kEnablePin, OUTPUT);

    digitalWrite(cfg::kStepPin, LOW);
    digitalWrite(cfg::kDirPin, LOW);

    position_ = position;
    accumulator_ = 0;
    increment_ = 0;
    direction_ = 0;

    enabled_ = false;
    applyEnable();

    // Timer1, CTC, prescaler 8. At 16 MHz that is a 2 MHz count, so the
    // compare value for cfg::kTickHz is 2e6 / kTickHz - 1.
    noInterrupts();
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0;
    OCR1A  = static_cast<uint16_t>((F_CPU / 8UL) / cfg::kTickHz - 1UL);
    TCCR1B |= (1 << WGM12);               // CTC
    TCCR1B |= (1 << CS11);                // prescaler 8
    TIMSK1 |= (1 << OCIE1A);              // compare match A interrupt
    interrupts();
}

void Stepper::setLimits(long min_steps, long max_steps) {
    min_steps_ = min_steps;
    max_steps_ = max_steps;
}

void Stepper::setVelocity(float steps_per_s) {
    const int8_t wanted = steps_per_s > 0.0f ? 1 : (steps_per_s < 0.0f ? -1 : 0);
    const uint32_t increment = motion::tickIncrement(steps_per_s, cfg::kTickHz);

    noInterrupts();

    if (wanted != 0 && wanted != direction_) {
        // Stop before turning around. The TB6600 wants DIR settled before the
        // next edge, and a pulse emitted against a stale direction would put
        // the position counter out of step with the real arm.
        increment_ = 0;
        accumulator_ = 0;
        direction_ = wanted;

        const bool level = cfg::kInvertDirection ? (wanted < 0) : (wanted > 0);
        digitalWrite(cfg::kDirPin, level ? HIGH : LOW);
    } else if (wanted == 0) {
        direction_ = 0;
    }

    increment_ = increment;

    interrupts();
}

long Stepper::position() const {
    noInterrupts();
    const long value = position_;
    interrupts();
    return value;
}

void Stepper::setPosition(long steps) {
    noInterrupts();
    position_ = steps;
    accumulator_ = 0;
    interrupts();
}

void Stepper::enable(bool on) {
    if (enabled_ == on) return;
    enabled_ = on;
    applyEnable();
}

void Stepper::applyEnable() {
    const bool active = cfg::kEnableActiveLow ? !enabled_ : enabled_;
    digitalWrite(cfg::kEnablePin, active ? HIGH : LOW);
}

void Stepper::emergencyStop() {
    noInterrupts();
    increment_ = 0;
    accumulator_ = 0;
    direction_ = 0;
    interrupts();

    enabled_ = false;
    applyEnable();
}

void Stepper::onTick() {
    if (increment_ == 0 || direction_ == 0) return;

    // Independent travel clamp. The planner decelerates well before this, so
    // reaching it means something upstream is wrong; stop rather than step.
    if (direction_ > 0 && position_ >= max_steps_) {
        increment_ = 0;
        return;
    }
    if (direction_ < 0 && position_ <= min_steps_) {
        increment_ = 0;
        return;
    }

    accumulator_ += increment_;
    if (accumulator_ < 0x10000UL) return;

    accumulator_ -= 0x10000UL;

    digitalWrite(cfg::kStepPin, HIGH);
    delayMicroseconds(cfg::kStepPulseUs);
    digitalWrite(cfg::kStepPin, LOW);

    position_ += direction_;
}

ISR(TIMER1_COMPA_vect) {
    Stepper* stepper = Stepper::instance();
    if (stepper != nullptr) stepper->onTick();
}
