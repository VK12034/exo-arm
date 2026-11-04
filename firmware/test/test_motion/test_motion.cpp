// Unit tests for the velocity planner, the step-rate arithmetic and the
// contact debouncing. Run with: pio test -e native
#include <unity.h>

#include "debounce.h"
#include "motion.h"

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

    return UNITY_END();
}
