// A small persistent record of what the arm has been doing, kept in EEPROM.
//
// Two things survive a power cycle badly on this machine. The first is a
// fault: the status LED blinks, the wearer switches off to get the arm away
// from themselves, and the reason is gone. The second is the drift figure the
// homing message reports, which only means something as a trend across
// sessions, and which docs/commissioning.md currently asks you to write down
// on paper.
//
// Both are now stored. There is no way to read the log back other than at
// power-on, where it is printed as part of the startup banner. That is
// deliberate: the alternative is a serial command, and accepting serial input
// at all would be the first path into this firmware that is not a button.
// Reading a log is harmless, but the parser that reads it is the thing worth
// not having.
//
// Writes are rare by construction, which is what makes EEPROM acceptable
// here. A drift record happens once per trip to the home switch and only if
// the correction was worth reporting; a fault record happens when something
// has already gone wrong. Routine e-stop presses are deliberately not
// recorded: they are normal operation, the wearer knows they pressed it, and
// logging them would fill the ring with noise and spend write cycles on it.
// Every byte goes through update semantics, so unchanged bytes are not
// rewritten.
#pragma once

#include <stdint.h>

class Journal {
public:
    enum class Event : uint8_t {
        kNone          = 0,  // empty slot
        kDrift         = 1,  // homing correction, value is signed steps
        kFaultOverload = 2,  // value is position in steps
        kFaultCollapse = 3,
        kFaultDrift    = 4,  // switch and counter disagreed beyond tolerance
        kFaultOverrun  = 5,  // flexion switch reached, value is the measured span
        kFaultSwitch   = 6,  // a switch closed where it physically cannot
    };

    struct Record {
        Event   event   = Event::kNone;
        uint16_t session = 0;
        int16_t  value   = 0;
    };

    // Injected so the host tests can back this with a plain array. On the Uno
    // these are EEPROM.read and EEPROM.write.
    using ReadFn  = uint8_t (*)(int address);
    using WriteFn = void (*)(int address, uint8_t value);

    static constexpr uint8_t  kCapacity = 16;
    static constexpr uint16_t kMagic    = 0x584A;  // "XJ"
    static constexpr uint8_t  kVersion  = 1;

    // Bytes of EEPROM this occupies from its base address.
    static constexpr int kFootprint = 6 + kCapacity * 5;

    // Reads the header, formats the store if it is absent or from another
    // version, then counts this power-on as a new session. Returns true if it
    // had to format, which the caller reports so a wiped log is not mistaken
    // for a clean history.
    bool begin(ReadFn read_fn, WriteFn write_fn, int base = 0) {
        read_ = read_fn;
        write_ = write_fn;
        base_ = base;

        const bool valid = (word(0) == kMagic) && (byteAt(2) == kVersion);
        bool formatted = false;

        if (!valid) {
            format();
            formatted = true;
        }

        next_ = byteAt(3);
        // A corrupt index is not worth faulting over; restart the ring.
        if (next_ >= kCapacity) next_ = 0;

        sessions_ = word(4);
        if (sessions_ < 0xFFFF) ++sessions_;  // saturate rather than wrap to zero
        putWord(4, sessions_);

        return formatted;
    }

    void recordDrift(int16_t steps) { append(Event::kDrift, steps); }

    void recordFault(Event event, int16_t position) {
        if (event == Event::kNone) return;
        append(event, position);
    }

    uint16_t sessions() const { return sessions_; }

    // Oldest first. Empty slots are skipped, so this is the number of records
    // actually held, up to kCapacity.
    uint8_t count() const {
        uint8_t n = 0;
        for (uint8_t i = 0; i < kCapacity; ++i) {
            if (recordAt(i).event != Event::kNone) ++n;
        }
        return n;
    }

    // Reads out in ring order, oldest surviving record first. Returns false
    // once there are no more.
    bool entry(uint8_t index, Record& out) const {
        uint8_t seen = 0;

        for (uint8_t i = 0; i < kCapacity; ++i) {
            const uint8_t slot = static_cast<uint8_t>((next_ + i) % kCapacity);
            const Record record = recordAt(slot);
            if (record.event == Event::kNone) continue;

            if (seen == index) {
                out = record;
                return true;
            }
            ++seen;
        }
        return false;
    }

private:
    static constexpr int kRecordBytes = 5;

    uint8_t byteAt(int offset) const { return read_(base_ + offset); }

    // Update semantics: a byte that already holds the wanted value is not
    // rewritten, because EEPROM cells wear out on writes and not on reads.
    void putByte(int offset, uint8_t value) {
        if (read_(base_ + offset) != value) write_(base_ + offset, value);
    }

    uint16_t word(int offset) const {
        return static_cast<uint16_t>(byteAt(offset)) |
               (static_cast<uint16_t>(byteAt(offset + 1)) << 8);
    }

    void putWord(int offset, uint16_t value) {
        putByte(offset, static_cast<uint8_t>(value & 0xFF));
        putByte(offset + 1, static_cast<uint8_t>(value >> 8));
    }

    Record recordAt(uint8_t slot) const {
        const int offset = 6 + slot * kRecordBytes;
        Record record;
        record.event = static_cast<Event>(byteAt(offset));
        record.session = word(offset + 1);
        record.value = static_cast<int16_t>(word(offset + 3));
        return record;
    }

    void append(Event event, int16_t value) {
        const int offset = 6 + next_ * kRecordBytes;

        putByte(offset, static_cast<uint8_t>(event));
        putWord(offset + 1, sessions_);
        putWord(offset + 3, static_cast<uint16_t>(value));

        next_ = static_cast<uint8_t>((next_ + 1) % kCapacity);
        putByte(3, next_);
    }

    void format() {
        putWord(0, kMagic);
        putByte(2, kVersion);
        putByte(3, 0);
        putWord(4, 0);

        for (uint8_t slot = 0; slot < kCapacity; ++slot) {
            const int offset = 6 + slot * kRecordBytes;
            for (int b = 0; b < kRecordBytes; ++b) putByte(offset + b, 0);
        }
    }

    ReadFn  read_  = nullptr;
    WriteFn write_ = nullptr;
    int     base_  = 0;

    uint8_t  next_     = 0;
    uint16_t sessions_ = 0;
};
