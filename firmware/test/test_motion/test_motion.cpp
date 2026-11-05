// Unit tests for the velocity planner, the step-rate arithmetic and the
// contact debouncing. Run with: pio test -e native
#include <unity.h>

#include "debounce.h"
#include "homing.h"
#include "journal.h"
#include "load.h"
#include "motion.h"
#include "overrun.h"

namespace {

// The real machine: 20 deg/s and 45 deg/s^2 at 66.667 steps per degree,
// travelling from full extension to 135 degrees of flexion.
constexpr float kStepsPerDeg = 66.6667f;
constexpr float kMaxSpeed    = 20.0f * kStepsPerDeg;
constexpr float kAccel       = 45.0f * kStepsPerDeg;
constexpr long  kMinSteps    = 0;
constexpr long  kMaxSteps    = static_cast<long>(135.0f * kStepsPerDeg);

constexpr float kDt = 0.001f;

motion::Planner makePlanner() {
    return motion::Planner(kMaxSpeed, kAccel, kMinSteps, kMaxSteps);
}

// Runs the planner against an integrated position for a fixed time, and
// returns where the arm ended up.
long run(motion::Planner& planner, motion::Command command, long start, int ms) {
    float position = static_cast<float>(start);

    for (int i = 0; i < ms; ++i) {
        planner.update(command, static_cast<long>(position), kDt);
        position += planner.velocity() * kDt;
    }
    return static_cast<long>(position);
}

}  // namespace

void setUp(void) {}
void tearDown(void) {}

// --- Planner ---------------------------------------------------------------

void test_starts_from_rest(void) {
    motion::Planner planner = makePlanner();
    TEST_ASSERT_FALSE(planner.moving());
    TEST_ASSERT_EQUAL_INT(0, planner.direction());
}

void test_hold_command_produces_no_motion(void) {
    motion::Planner planner = makePlanner();
    const long end = run(planner, motion::Command::kHold, 2000, 500);

    TEST_ASSERT_EQUAL_INT32(2000, end);
    TEST_ASSERT_FALSE(planner.moving());
}

void test_flex_ramps_up_rather_than_stepping(void) {
    motion::Planner planner = makePlanner();

    // After a single millisecond the velocity must be one acceleration step,
    // not the full commanded speed.
    planner.update(motion::Command::kFlex, 2000, kDt);

    TEST_ASSERT_FLOAT_WITHIN(1.0f, kAccel * kDt, planner.velocity());
    TEST_ASSERT_TRUE(planner.velocity() < kMaxSpeed);
}

void test_flex_reaches_but_does_not_exceed_max_speed(void) {
    motion::Planner planner = makePlanner();
    run(planner, motion::Command::kFlex, 2000, 2000);

    TEST_ASSERT_FLOAT_WITHIN(1.0f, kMaxSpeed, planner.velocity());
    TEST_ASSERT_EQUAL_INT(1, planner.direction());
}

void test_extend_drives_negative(void) {
    motion::Planner planner = makePlanner();
    run(planner, motion::Command::kExtend, 5000, 2000);

    TEST_ASSERT_FLOAT_WITHIN(1.0f, -kMaxSpeed, planner.velocity());
    TEST_ASSERT_EQUAL_INT(-1, planner.direction());
}

void test_releasing_the_button_ramps_down_to_a_stop(void) {
    motion::Planner planner = makePlanner();
    run(planner, motion::Command::kFlex, 2000, 2000);
    TEST_ASSERT_TRUE(planner.moving());

    run(planner, motion::Command::kHold, 4000, 2000);

    TEST_ASSERT_FALSE(planner.moving());
    TEST_ASSERT_EQUAL_INT(0, planner.direction());
}

void test_stops_before_the_upper_limit(void) {
    motion::Planner planner = makePlanner();

    // Hold flexion far longer than it takes to cross the whole travel.
    const long end = run(planner, motion::Command::kFlex, kMinSteps, 20000);

    TEST_ASSERT_FALSE(planner.moving());
    TEST_ASSERT_TRUE(end <= kMaxSteps);
}

void test_stops_before_the_lower_limit(void) {
    motion::Planner planner = makePlanner();
    const long end = run(planner, motion::Command::kExtend, kMaxSteps, 20000);

    TEST_ASSERT_FALSE(planner.moving());
    TEST_ASSERT_TRUE(end >= kMinSteps);
}

void test_arriving_at_a_limit_leaves_no_residual_speed(void) {
    motion::Planner planner = makePlanner();
    run(planner, motion::Command::kFlex, kMinSteps, 20000);

    // Coming to rest at the limit matters more than reaching it: a stepper
    // stopped dead at speed loses steps, and the counter is all we have.
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, planner.velocity());
}

void test_refuses_to_push_past_a_limit_already_reached(void) {
    motion::Planner planner = makePlanner();
    run(planner, motion::Command::kFlex, kMaxSteps, 500);

    TEST_ASSERT_FALSE(planner.moving());
    TEST_ASSERT_TRUE(planner.blockedAbove(kMaxSteps));
}

void test_can_always_drive_away_from_a_limit(void) {
    motion::Planner planner = makePlanner();
    run(planner, motion::Command::kExtend, kMaxSteps, 500);

    // The wearer must never be trapped against a limit.
    TEST_ASSERT_TRUE(planner.velocity() < 0.0f);
}

void test_reversal_passes_through_zero(void) {
    motion::Planner planner = makePlanner();
    run(planner, motion::Command::kFlex, 2000, 2000);
    TEST_ASSERT_TRUE(planner.velocity() > 0.0f);

    // Step the reversal one millisecond at a time and watch for a sign flip
    // that skips zero, which would be a direction change under power.
    float previous = planner.velocity();
    bool crossed_cleanly = true;

    for (int i = 0; i < 3000; ++i) {
        planner.update(motion::Command::kExtend, 2000, kDt);
        const float now = planner.velocity();

        if (previous > 0.0f && now < 0.0f) crossed_cleanly = false;
        if (now < -1.0f) break;
        previous = now;
    }

    TEST_ASSERT_TRUE(crossed_cleanly);
}

void test_emergency_stop_zeroes_velocity(void) {
    motion::Planner planner = makePlanner();
    run(planner, motion::Command::kFlex, 2000, 2000);
    TEST_ASSERT_TRUE(planner.moving());

    planner.stop();

    TEST_ASSERT_FALSE(planner.moving());
}

void test_stopping_distance(void) {
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 25.0f, motion::Planner::stoppingDistance(100.0f, 200.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 25.0f, motion::Planner::stoppingDistance(-100.0f, 200.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, motion::Planner::stoppingDistance(100.0f, 0.0f));
}

// --- Step-rate arithmetic --------------------------------------------------

void test_tick_increment_scales_with_velocity(void) {
    TEST_ASSERT_EQUAL_UINT32(0u, motion::tickIncrement(0.0f, 10000));
    TEST_ASSERT_EQUAL_UINT32(65536u, motion::tickIncrement(10000.0f, 10000));
    TEST_ASSERT_EQUAL_UINT32(32768u, motion::tickIncrement(5000.0f, 10000));
}

void test_tick_increment_ignores_sign(void) {
    TEST_ASSERT_EQUAL_UINT32(
        motion::tickIncrement(1333.0f, 10000), motion::tickIncrement(-1333.0f, 10000));
}

void test_tick_increment_clamps_at_one_step_per_tick(void) {
    // Asking for more than one step per tick would silently drop steps, and
    // the position counter would stop matching the real arm.
    TEST_ASSERT_EQUAL_UINT32(65536u, motion::tickIncrement(999999.0f, 10000));
}

void test_tick_increment_at_the_real_top_speed(void) {
    // 20 deg/s is about 1333 steps/s, comfortably under the tick rate.
    const uint32_t increment = motion::tickIncrement(kMaxSpeed, 10000);

    TEST_ASSERT_TRUE(increment > 0u);
    TEST_ASSERT_TRUE(increment < 65536u);
}

// --- Debounce --------------------------------------------------------------

void test_debounce_ignores_a_short_bounce(void) {
    Debounced button(25);
    button.reset(false, 0);

    button.update(true, 5);
    button.update(false, 10);
    button.update(true, 15);

    TEST_ASSERT_FALSE(button.state());
}

void test_debounce_settles_after_the_interval(void) {
    Debounced button(25);
    button.reset(false, 0);

    button.update(true, 100);
    TEST_ASSERT_FALSE(button.state());

    button.update(true, 130);
    TEST_ASSERT_TRUE(button.state());
    TEST_ASSERT_TRUE(button.rose());
}

void test_debounce_reports_release(void) {
    Debounced button(25);
    button.reset(true, 0);

    button.update(false, 100);
    button.update(false, 140);

    TEST_ASSERT_FALSE(button.state());
    TEST_ASSERT_TRUE(button.fell());
}

void test_debounce_reset_does_not_report_an_edge(void) {
    Debounced button(25);
    button.reset(true, 0);

    // A button already held at power-on must not read as a fresh press.
    TEST_ASSERT_TRUE(button.state());
    TEST_ASSERT_FALSE(button.rose());
}

void test_debounce_tracks_hold_time(void) {
    Debounced button(25);
    button.reset(false, 0);

    button.update(true, 100);
    button.update(true, 130);

    TEST_ASSERT_EQUAL_UINT32(0u, button.heldMs(130));
    TEST_ASSERT_EQUAL_UINT32(3000u, button.heldMs(3130));
}

// --- Load monitor ----------------------------------------------------------

namespace {

// Thresholds from config.h, with the filter wide open so these tests
// exercise the decision logic rather than the smoothing.
LoadMonitor makeLoad() {
    LoadMonitor load;
    load.configure(2.5f, 400, 0.15f, 600, 250, 1.0f);
    load.reset(0.0f);
    return load;
}

uint32_t feed(LoadMonitor& load, float amps, bool moving, uint32_t from_ms, uint32_t ms) {
    for (uint32_t t = 0; t < ms; ++t) {
        load.update(amps, moving, from_ms + t);
    }
    return from_ms + ms;
}

}  // namespace

void test_load_ignores_current_while_stopped(void) {
    LoadMonitor load = makeLoad();

    // A de-energised driver says nothing about load, however the reading looks.
    feed(load, 9.0f, false, 0, 5000);

    TEST_ASSERT_FALSE(load.tripped());
}

void test_load_allows_inrush_during_the_grace_period(void) {
    LoadMonitor load = makeLoad();

    feed(load, 6.0f, true, 0, 200);

    TEST_ASSERT_FALSE(load.tripped());
}

void test_load_trips_on_sustained_overload(void) {
    LoadMonitor load = makeLoad();

    const uint32_t t = feed(load, 4.0f, true, 0, 300);
    TEST_ASSERT_FALSE(load.tripped());

    feed(load, 4.0f, true, t, 500);

    TEST_ASSERT_TRUE(load.overload());
    TEST_ASSERT_FALSE(load.collapse());
}

void test_load_ignores_a_brief_overload_spike(void) {
    LoadMonitor load = makeLoad();

    uint32_t t = feed(load, 1.0f, true, 0, 400);
    t = feed(load, 4.0f, true, t, 200);   // spike, shorter than kOverloadMs
    feed(load, 1.0f, true, t, 500);

    TEST_ASSERT_FALSE(load.tripped());
}

void test_load_trips_on_current_collapse(void) {
    LoadMonitor load = makeLoad();

    uint32_t t = feed(load, 1.0f, true, 0, 400);
    feed(load, 0.05f, true, t, 800);

    TEST_ASSERT_TRUE(load.collapse());
    TEST_ASSERT_FALSE(load.overload());
}

void test_load_accepts_normal_working_current(void) {
    LoadMonitor load = makeLoad();

    feed(load, 1.2f, true, 0, 10000);

    TEST_ASSERT_FALSE(load.tripped());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.2f, load.current());
}

void test_load_timers_restart_when_motion_stops(void) {
    LoadMonitor load = makeLoad();

    // Most of an overload, then a pause, then most of one again. Neither
    // stretch is long enough on its own and they must not add up.
    uint32_t t = feed(load, 4.0f, true, 0, 300);
    t = feed(load, 0.0f, false, t, 100);
    t = feed(load, 4.0f, true, t, 300);

    TEST_ASSERT_FALSE(load.tripped());
}

void test_load_clear_releases_the_trip(void) {
    LoadMonitor load = makeLoad();

    feed(load, 4.0f, true, 0, 800);
    TEST_ASSERT_TRUE(load.tripped());

    load.clear();

    TEST_ASSERT_FALSE(load.tripped());
    TEST_ASSERT_FALSE(load.overload());
}

void test_load_filter_smooths_the_reading(void) {
    LoadMonitor load;
    load.configure(2.5f, 400, 0.15f, 600, 250, 0.05f);
    load.reset(0.0f);

    // One sample at the chopper's peak must not move the filtered value far.
    load.update(5.0f, true, 0);

    TEST_ASSERT_TRUE(load.current() < 0.5f);
}


// --- Homing ----------------------------------------------------------------

namespace {

// Half a degree of tolerance, five degrees before it is treated as lost
// steps, at the real 66.667 steps per degree.
constexpr long kHomeSteps = 0;
constexpr long kTolerance = static_cast<long>(0.5f * kStepsPerDeg);
constexpr long kFaultSteps = static_cast<long>(5.0f * kStepsPerDeg);

Homing makeHoming() {
    Homing homing;
    homing.configure(kHomeSteps, kTolerance, kFaultSteps);
    homing.reset();
    return homing;
}

}  // namespace

void test_homing_starts_unhomed(void) {
    Homing homing = makeHoming();

    TEST_ASSERT_FALSE(homing.homed());
    TEST_ASSERT_TRUE(Homing::Source::kNone == homing.source());
}

void test_homing_does_nothing_until_the_switch_is_reached(void) {
    Homing homing = makeHoming();

    TEST_ASSERT_TRUE(Homing::Action::kNone == homing.update(false, -1000));
    TEST_ASSERT_FALSE(homing.homed());
}

// An arm parked against the switch at power-on must home on the first update,
// not have to leave the switch and come back to it.
void test_homing_adopts_a_switch_already_closed_at_startup(void) {
    Homing homing = makeHoming();

    TEST_ASSERT_TRUE(Homing::Action::kZero == homing.update(true, 0));
    TEST_ASSERT_TRUE(homing.homed());
    TEST_ASSERT_TRUE(Homing::Source::kSwitch == homing.source());
}

// Before homing the counter means nothing, so reaching the switch a long way
// from nominal zero is the reference being established, not a discrepancy.
void test_first_home_is_a_definition_not_a_measurement(void) {
    Homing homing = makeHoming();

    TEST_ASSERT_TRUE(Homing::Action::kZero == homing.update(true, -4000));
    TEST_ASSERT_EQUAL_INT32(0, homing.drift());
    TEST_ASSERT_FALSE(homing.driftSignificant());
}

void test_homing_does_not_rezero_while_the_switch_stays_closed(void) {
    Homing homing = makeHoming();
    homing.update(true, 0);

    TEST_ASSERT_TRUE(Homing::Action::kNone == homing.update(true, 0));
    TEST_ASSERT_TRUE(Homing::Action::kNone == homing.update(true, 10));
}

void test_homing_corrects_a_small_drift_silently(void) {
    Homing homing = makeHoming();
    homing.update(true, 0);
    homing.update(false, 2000);

    // Back on the switch a few steps out: rounding, not lost steps.
    TEST_ASSERT_TRUE(Homing::Action::kZero == homing.update(true, 12));
    TEST_ASSERT_EQUAL_INT32(12, homing.drift());
    TEST_ASSERT_FALSE(homing.driftSignificant());
    TEST_ASSERT_TRUE(homing.homed());
}

void test_homing_reports_a_drift_worth_knowing_about(void) {
    Homing homing = makeHoming();
    homing.update(true, 0);
    homing.update(false, 2000);

    // Two degrees out: inside the fault threshold, so correct it, but say so.
    const long drift = static_cast<long>(2.0f * kStepsPerDeg);
    TEST_ASSERT_TRUE(Homing::Action::kZero == homing.update(true, drift));
    TEST_ASSERT_EQUAL_INT32(drift, homing.drift());
    TEST_ASSERT_TRUE(homing.driftSignificant());
    TEST_ASSERT_TRUE(homing.homed());
}

// A large disagreement means the far limit has moved too, so correcting the
// zero would hide the real problem.
void test_homing_faults_when_the_counter_has_lost_too_much(void) {
    Homing homing = makeHoming();
    homing.update(true, 0);
    homing.update(false, 4000);

    const long drift = static_cast<long>(12.0f * kStepsPerDeg);
    TEST_ASSERT_TRUE(Homing::Action::kFault == homing.update(true, drift));
    TEST_ASSERT_FALSE(homing.homed());
    TEST_ASSERT_TRUE(Homing::Source::kNone == homing.source());
}

void test_homing_faults_on_drift_in_either_direction(void) {
    Homing homing = makeHoming();
    homing.update(true, 0);
    homing.update(false, -4000);

    const long drift = static_cast<long>(-12.0f * kStepsPerDeg);
    TEST_ASSERT_TRUE(Homing::Action::kFault == homing.update(true, drift));
    TEST_ASSERT_FALSE(homing.homed());
}

// A normally-open switch cannot read closed by being broken, only by being
// shorted or jammed shut, and a jammed one would re-zero the arm mid-travel.
void test_homing_faults_on_a_switch_stuck_closed(void) {
    Homing homing = makeHoming();
    homing.update(true, 0);

    const long far = static_cast<long>(60.0f * kStepsPerDeg);
    TEST_ASSERT_TRUE(Homing::Action::kFault == homing.update(true, far));
}

void test_operator_zero_is_recorded_as_asserted(void) {
    Homing homing = makeHoming();
    homing.adoptOperatorZero();

    TEST_ASSERT_TRUE(homing.homed());
    TEST_ASSERT_TRUE(Homing::Source::kOperator == homing.source());
    TEST_ASSERT_EQUAL_INT32(0, homing.drift());
}

// After an operator zero the switch is still the better reference, so
// reaching it must take over.
void test_switch_supersedes_an_operator_zero(void) {
    Homing homing = makeHoming();
    homing.adoptOperatorZero();

    TEST_ASSERT_TRUE(Homing::Action::kZero == homing.update(true, 10));
    TEST_ASSERT_TRUE(Homing::Source::kSwitch == homing.source());
}

void test_homing_reset_returns_to_unhomed(void) {
    Homing homing = makeHoming();
    homing.update(true, 0);
    homing.reset();

    TEST_ASSERT_FALSE(homing.homed());
    TEST_ASSERT_TRUE(Homing::Source::kNone == homing.source());
}


// --- Journal ---------------------------------------------------------------

namespace {

// Stands in for the Uno's EEPROM. Writes are counted so the update semantics
// can be checked: EEPROM cells wear out on writes, so a byte that already
// holds the wanted value must not be rewritten.
constexpr int kFakeEepromSize = 1024;
uint8_t g_fake_eeprom[kFakeEepromSize];
int     g_fake_writes = 0;

uint8_t fakeRead(int address) { return g_fake_eeprom[address]; }

void fakeWrite(int address, uint8_t value) {
    g_fake_eeprom[address] = value;
    ++g_fake_writes;
}

// Fresh EEPROM from the factory reads 0xFF, which is what the magic check has
// to reject.
void wipeFakeEeprom(uint8_t fill) {
    for (int i = 0; i < kFakeEepromSize; ++i) g_fake_eeprom[i] = fill;
    g_fake_writes = 0;
}

Journal openJournal() {
    Journal journal;
    journal.begin(fakeRead, fakeWrite, 0);
    return journal;
}

}  // namespace

void test_journal_formats_a_blank_store(void) {
    wipeFakeEeprom(0xFF);

    Journal journal;
    TEST_ASSERT_TRUE(journal.begin(fakeRead, fakeWrite, 0));
    TEST_ASSERT_EQUAL_INT(0, journal.count());
    TEST_ASSERT_EQUAL_INT(1, journal.sessions());
}

void test_journal_does_not_reformat_a_valid_store(void) {
    wipeFakeEeprom(0xFF);
    openJournal();

    Journal second;
    TEST_ASSERT_FALSE(second.begin(fakeRead, fakeWrite, 0));
}

// The session number is what makes a drift trend readable, so it has to
// survive a power cycle and advance exactly once per boot.
void test_journal_counts_sessions_across_power_cycles(void) {
    wipeFakeEeprom(0xFF);

    openJournal();
    openJournal();
    Journal third = openJournal();

    TEST_ASSERT_EQUAL_INT(3, third.sessions());
}

void test_journal_saturates_the_session_counter(void) {
    wipeFakeEeprom(0xFF);
    openJournal();

    g_fake_eeprom[4] = 0xFF;
    g_fake_eeprom[5] = 0xFF;

    Journal journal = openJournal();
    TEST_ASSERT_EQUAL_INT(0xFFFF, journal.sessions());
}

void test_journal_records_and_reads_back_a_drift(void) {
    wipeFakeEeprom(0xFF);
    Journal journal = openJournal();

    journal.recordDrift(-164);

    Journal::Record record;
    TEST_ASSERT_EQUAL_INT(1, journal.count());
    TEST_ASSERT_TRUE(journal.entry(0, record));
    TEST_ASSERT_TRUE(Journal::Event::kDrift == record.event);
    TEST_ASSERT_EQUAL_INT32(-164, record.value);
    TEST_ASSERT_EQUAL_INT(1, record.session);
}

void test_journal_records_a_fault_with_its_position(void) {
    wipeFakeEeprom(0xFF);
    Journal journal = openJournal();

    journal.recordFault(Journal::Event::kFaultOverload, 4200);

    Journal::Record record;
    TEST_ASSERT_TRUE(journal.entry(0, record));
    TEST_ASSERT_TRUE(Journal::Event::kFaultOverload == record.event);
    TEST_ASSERT_EQUAL_INT32(4200, record.value);
}

// kNone is the empty-slot marker, so recording it would punch a hole in the
// ring rather than add an entry.
void test_journal_ignores_an_empty_event(void) {
    wipeFakeEeprom(0xFF);
    Journal journal = openJournal();

    journal.recordFault(Journal::Event::kNone, 10);
    TEST_ASSERT_EQUAL_INT(0, journal.count());
}

void test_journal_survives_a_power_cycle(void) {
    wipeFakeEeprom(0xFF);
    {
        Journal journal = openJournal();
        journal.recordDrift(33);
        journal.recordFault(Journal::Event::kFaultCollapse, 900);
    }

    Journal journal = openJournal();
    TEST_ASSERT_EQUAL_INT(2, journal.count());

    Journal::Record record;
    TEST_ASSERT_TRUE(journal.entry(1, record));
    TEST_ASSERT_TRUE(Journal::Event::kFaultCollapse == record.event);
    TEST_ASSERT_EQUAL_INT32(900, record.value);
}

void test_journal_reads_oldest_first(void) {
    wipeFakeEeprom(0xFF);
    Journal journal = openJournal();

    journal.recordDrift(1);
    journal.recordDrift(2);
    journal.recordDrift(3);

    Journal::Record record;
    for (int16_t expected = 1; expected <= 3; ++expected) {
        TEST_ASSERT_TRUE(journal.entry(static_cast<uint8_t>(expected - 1), record));
        TEST_ASSERT_EQUAL_INT32(expected, record.value);
    }
}

// Once the ring is full the oldest records go, not the newest. A log that
// drops the most recent fault would be worse than no log.
void test_journal_ring_keeps_the_newest_records(void) {
    wipeFakeEeprom(0xFF);
    Journal journal = openJournal();

    const int total = Journal::kCapacity + 3;
    for (int i = 1; i <= total; ++i) {
        journal.recordDrift(static_cast<int16_t>(i));
    }

    TEST_ASSERT_EQUAL_INT(Journal::kCapacity, journal.count());

    Journal::Record record;
    TEST_ASSERT_TRUE(journal.entry(0, record));
    TEST_ASSERT_EQUAL_INT32(total - Journal::kCapacity + 1, record.value);

    TEST_ASSERT_TRUE(journal.entry(Journal::kCapacity - 1, record));
    TEST_ASSERT_EQUAL_INT32(total, record.value);
}

void test_journal_entry_reports_the_end(void) {
    wipeFakeEeprom(0xFF);
    Journal journal = openJournal();
    journal.recordDrift(5);

    Journal::Record record;
    TEST_ASSERT_TRUE(journal.entry(0, record));
    TEST_ASSERT_FALSE(journal.entry(1, record));
}

// EEPROM endurance is finite, so rewriting a byte that already holds the
// wanted value is a bug and not merely wasteful.
void test_journal_does_not_rewrite_unchanged_bytes(void) {
    wipeFakeEeprom(0xFF);
    Journal journal = openJournal();
    journal.recordDrift(7);

    g_fake_writes = 0;
    journal.recordDrift(7);

    // The event byte and the value are already right from the record before,
    // so only the session, the ring index and whatever genuinely differs can
    // account for any write at all.
    TEST_ASSERT_TRUE(g_fake_writes < 5);
}

void test_journal_clamps_a_corrupt_ring_index(void) {
    wipeFakeEeprom(0xFF);
    openJournal();

    g_fake_eeprom[3] = 200;  // beyond kCapacity

    Journal journal = openJournal();
    journal.recordDrift(99);

    Journal::Record record;
    TEST_ASSERT_TRUE(journal.entry(0, record));
    TEST_ASSERT_EQUAL_INT32(99, record.value);
}

void test_journal_footprint_fits_the_uno(void) {
    // The Uno has 1 KB. Anything approaching that is a design error here.
    TEST_ASSERT_TRUE(Journal::kFootprint < 256);
}


// --- Overrun guard ---------------------------------------------------------

namespace {

// The switch sits at 138 degrees, three degrees past the 135 degree soft
// limit, with two degrees of tolerance on the span measurement.
constexpr long kFlexSteps     = static_cast<long>(138.0f * kStepsPerDeg);
constexpr long kSpanTolerance = static_cast<long>(2.0f * kStepsPerDeg);
constexpr long kMinPlausible  = kFlexSteps / 2;

OverrunGuard makeGuard() {
    OverrunGuard guard;
    guard.configure(kFlexSteps, kSpanTolerance, kMinPlausible);
    guard.reset();
    return guard;
}

}  // namespace

void test_overrun_quiet_while_the_switch_is_open(void) {
    OverrunGuard guard = makeGuard();

    TEST_ASSERT_TRUE(OverrunGuard::Action::kNone == guard.update(false, 0));
    TEST_ASSERT_TRUE(OverrunGuard::Action::kNone == guard.update(false, kMaxSteps));
}

void test_overrun_fires_when_the_switch_is_reached(void) {
    OverrunGuard guard = makeGuard();

    TEST_ASSERT_TRUE(OverrunGuard::Action::kOverrun ==
                     guard.update(true, kFlexSteps));
    TEST_ASSERT_TRUE(guard.measured());
    TEST_ASSERT_EQUAL_INT32(kFlexSteps, guard.span());
}

// Reaching it once is a fault; holding it closed afterwards must not keep
// re-faulting while the wearer extends back out of it.
void test_overrun_fires_once_per_closure(void) {
    OverrunGuard guard = makeGuard();
    guard.update(true, kFlexSteps);

    TEST_ASSERT_TRUE(OverrunGuard::Action::kNone == guard.update(true, kFlexSteps));
    TEST_ASSERT_TRUE(OverrunGuard::Action::kNone == guard.update(true, kFlexSteps - 50));
}

void test_overrun_rearms_after_the_switch_opens(void) {
    OverrunGuard guard = makeGuard();
    guard.update(true, kFlexSteps);
    guard.update(false, kMaxSteps);

    TEST_ASSERT_TRUE(OverrunGuard::Action::kOverrun ==
                     guard.update(true, kFlexSteps));
}

// A switch at the flexion end cannot be reached while the counter reads near
// zero. Shorted, on the wrong pin, or the span would be meaningless anyway.
void test_overrun_rejects_an_implausible_closure(void) {
    OverrunGuard guard = makeGuard();

    TEST_ASSERT_TRUE(OverrunGuard::Action::kImplausible == guard.update(true, 0));
    TEST_ASSERT_FALSE(guard.measured());
}

void test_overrun_treats_a_boot_time_closure_as_implausible(void) {
    // Position starts at nominal zero, so a switch already closed at power-on
    // must be caught rather than read as a 0 step span.
    OverrunGuard guard = makeGuard();

    TEST_ASSERT_TRUE(OverrunGuard::Action::kImplausible == guard.update(true, 0));
}

void test_overrun_span_agrees_when_the_siting_is_right(void) {
    OverrunGuard guard = makeGuard();
    guard.update(true, kFlexSteps + 40);  // inside two degrees

    TEST_ASSERT_FALSE(guard.spanSignificant());
    TEST_ASSERT_EQUAL_INT32(40, guard.spanError());
}

void test_overrun_span_error_is_signed(void) {
    OverrunGuard guard = makeGuard();
    guard.update(true, kFlexSteps - 500);

    TEST_ASSERT_EQUAL_INT32(-500, guard.spanError());
    TEST_ASSERT_TRUE(guard.spanSignificant());
}

// The case this measurement exists for: the TB6600 set to 1/16 while
// config.h says 1/8 makes every commanded step half a real step, so the arm
// travels only half as far before the switch and the span reads half.
void test_overrun_span_catches_a_microstep_mismatch(void) {
    OverrunGuard guard = makeGuard();
    guard.update(true, kFlexSteps / 2);

    TEST_ASSERT_TRUE(guard.measured());
    TEST_ASSERT_TRUE(guard.spanSignificant());

    // Half the span is far outside any tolerance that could absorb it.
    const long error = guard.spanError();
    TEST_ASSERT_TRUE(error < -kSpanTolerance * 4);
}

void test_overrun_reset_clears_the_measurement(void) {
    OverrunGuard guard = makeGuard();
    guard.update(true, kFlexSteps);
    guard.reset();

    TEST_ASSERT_FALSE(guard.measured());
    TEST_ASSERT_EQUAL_INT32(0, guard.span());
}

// The switch is three degrees past the soft limit, and the planner comes to
// rest at the limit. A normal full-flexion stop must not touch it.
void test_overrun_switch_sits_clear_of_a_normal_stop(void) {
    motion::Planner planner = makePlanner();
    const long end = run(planner, motion::Command::kFlex, kMaxSteps - 2000, 4000);

    TEST_ASSERT_TRUE(end <= kMaxSteps);
    TEST_ASSERT_TRUE(end < kFlexSteps);
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_starts_from_rest);
    RUN_TEST(test_hold_command_produces_no_motion);
    RUN_TEST(test_flex_ramps_up_rather_than_stepping);
    RUN_TEST(test_flex_reaches_but_does_not_exceed_max_speed);
    RUN_TEST(test_extend_drives_negative);
    RUN_TEST(test_releasing_the_button_ramps_down_to_a_stop);
    RUN_TEST(test_stops_before_the_upper_limit);
    RUN_TEST(test_stops_before_the_lower_limit);
    RUN_TEST(test_arriving_at_a_limit_leaves_no_residual_speed);
    RUN_TEST(test_refuses_to_push_past_a_limit_already_reached);
    RUN_TEST(test_can_always_drive_away_from_a_limit);
    RUN_TEST(test_reversal_passes_through_zero);
    RUN_TEST(test_emergency_stop_zeroes_velocity);
    RUN_TEST(test_stopping_distance);

    RUN_TEST(test_tick_increment_scales_with_velocity);
    RUN_TEST(test_tick_increment_ignores_sign);
    RUN_TEST(test_tick_increment_clamps_at_one_step_per_tick);
    RUN_TEST(test_tick_increment_at_the_real_top_speed);

    RUN_TEST(test_debounce_ignores_a_short_bounce);
    RUN_TEST(test_debounce_settles_after_the_interval);
    RUN_TEST(test_debounce_reports_release);
    RUN_TEST(test_debounce_reset_does_not_report_an_edge);
    RUN_TEST(test_debounce_tracks_hold_time);

    RUN_TEST(test_load_ignores_current_while_stopped);
    RUN_TEST(test_load_allows_inrush_during_the_grace_period);
    RUN_TEST(test_load_trips_on_sustained_overload);
    RUN_TEST(test_load_ignores_a_brief_overload_spike);
    RUN_TEST(test_load_trips_on_current_collapse);
    RUN_TEST(test_load_accepts_normal_working_current);
    RUN_TEST(test_load_timers_restart_when_motion_stops);
    RUN_TEST(test_load_clear_releases_the_trip);
    RUN_TEST(test_load_filter_smooths_the_reading);

    RUN_TEST(test_homing_starts_unhomed);
    RUN_TEST(test_homing_does_nothing_until_the_switch_is_reached);
    RUN_TEST(test_homing_adopts_a_switch_already_closed_at_startup);
    RUN_TEST(test_first_home_is_a_definition_not_a_measurement);
    RUN_TEST(test_homing_does_not_rezero_while_the_switch_stays_closed);
    RUN_TEST(test_homing_corrects_a_small_drift_silently);
    RUN_TEST(test_homing_reports_a_drift_worth_knowing_about);
    RUN_TEST(test_homing_faults_when_the_counter_has_lost_too_much);
    RUN_TEST(test_homing_faults_on_drift_in_either_direction);
    RUN_TEST(test_homing_faults_on_a_switch_stuck_closed);
    RUN_TEST(test_operator_zero_is_recorded_as_asserted);
    RUN_TEST(test_switch_supersedes_an_operator_zero);
    RUN_TEST(test_homing_reset_returns_to_unhomed);

    RUN_TEST(test_journal_formats_a_blank_store);
    RUN_TEST(test_journal_does_not_reformat_a_valid_store);
    RUN_TEST(test_journal_counts_sessions_across_power_cycles);
    RUN_TEST(test_journal_saturates_the_session_counter);
    RUN_TEST(test_journal_records_and_reads_back_a_drift);
    RUN_TEST(test_journal_records_a_fault_with_its_position);
    RUN_TEST(test_journal_ignores_an_empty_event);
    RUN_TEST(test_journal_survives_a_power_cycle);
    RUN_TEST(test_journal_reads_oldest_first);
    RUN_TEST(test_journal_ring_keeps_the_newest_records);
    RUN_TEST(test_journal_entry_reports_the_end);
    RUN_TEST(test_journal_does_not_rewrite_unchanged_bytes);
    RUN_TEST(test_journal_clamps_a_corrupt_ring_index);
    RUN_TEST(test_journal_footprint_fits_the_uno);

    RUN_TEST(test_overrun_quiet_while_the_switch_is_open);
    RUN_TEST(test_overrun_fires_when_the_switch_is_reached);
    RUN_TEST(test_overrun_fires_once_per_closure);
    RUN_TEST(test_overrun_rearms_after_the_switch_opens);
    RUN_TEST(test_overrun_rejects_an_implausible_closure);
    RUN_TEST(test_overrun_treats_a_boot_time_closure_as_implausible);
    RUN_TEST(test_overrun_span_agrees_when_the_siting_is_right);
    RUN_TEST(test_overrun_span_error_is_signed);
    RUN_TEST(test_overrun_span_catches_a_microstep_mismatch);
    RUN_TEST(test_overrun_reset_clears_the_measurement);
    RUN_TEST(test_overrun_switch_sits_clear_of_a_normal_stop);

    return UNITY_END();
}
