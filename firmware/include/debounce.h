// Contact debouncing, free of Arduino headers so the tests can drive it with
// a synthetic clock.
#pragma once

#include <stdint.h>

class Debounced {
public:
    Debounced() = default;
    explicit Debounced(uint16_t interval_ms) : interval_ms_(interval_ms) {}

    void configure(uint16_t interval_ms) { interval_ms_ = interval_ms; }

    // Seeds the state without reporting an edge. Call once at startup so a
    // button already held at power-on does not read as a fresh press.
    void reset(bool level, uint32_t now_ms) {
        state_ = level;
        candidate_ = level;
        changed_at_ = now_ms;
        since_ = now_ms;
        rose_ = false;
        fell_ = false;
    }

    void update(bool level, uint32_t now_ms) {
        rose_ = false;
        fell_ = false;

        if (level != candidate_) {
            candidate_ = level;
            changed_at_ = now_ms;
            return;
        }

        if (level == state_) return;

        if (now_ms - changed_at_ < interval_ms_) return;

        state_ = level;
        since_ = now_ms;
        rose_ = level;
        fell_ = !level;
    }

    bool state() const { return state_; }
    bool rose() const { return rose_; }
    bool fell() const { return fell_; }

    // How long the current state has been held, in milliseconds.
    uint32_t heldMs(uint32_t now_ms) const { return now_ms - since_; }

private:
    uint16_t interval_ms_ = 25;

    bool state_     = false;
    bool candidate_ = false;
    bool rose_      = false;
    bool fell_      = false;

    uint32_t changed_at_ = 0;
    uint32_t since_      = 0;
};
