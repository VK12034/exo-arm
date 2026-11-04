#include "pendant.h"

void Pendant::begin(uint32_t now_ms) {
    pinMode(cfg::kUpPin, INPUT_PULLUP);
    pinMode(cfg::kDownPin, INPUT_PULLUP);
    pinMode(cfg::kEStopPin, INPUT_PULLUP);

    up_.configure(cfg::kDebounceMs);
    down_.configure(cfg::kDebounceMs);
    estop_.configure(cfg::kDebounceMs);

    // Seed from the real pin states so a button held through power-on is not
    // reported as a press, and so a tripped e-stop is known immediately.
    up_.reset(digitalRead(cfg::kUpPin) == LOW, now_ms);
    down_.reset(digitalRead(cfg::kDownPin) == LOW, now_ms);
    estop_.reset(digitalRead(cfg::kEStopPin) == HIGH, now_ms);

    estop_open_ = estop_.state();
    both_held_  = false;
    both_since_ = now_ms;
}

void Pendant::poll(uint32_t now_ms) {
    up_.update(digitalRead(cfg::kUpPin) == LOW, now_ms);
    down_.update(digitalRead(cfg::kDownPin) == LOW, now_ms);

    // Closed loop pulls the pin to ground, so HIGH is the open, stop state.
    estop_.update(digitalRead(cfg::kEStopPin) == HIGH, now_ms);
    estop_open_ = estop_.state();

    const bool both = up_.state() && down_.state();
    if (both && !both_held_) {
        both_since_ = now_ms;
    }
    both_held_ = both;
}

motion::Command Pendant::command() const {
    if (up_.state() == down_.state()) {
        return motion::Command::kHold;
    }
    return up_.state() ? motion::Command::kFlex : motion::Command::kExtend;
}

uint32_t Pendant::bothHeldMs(uint32_t now_ms) const {
    if (!both_held_) return 0;
    return now_ms - both_since_;
}
