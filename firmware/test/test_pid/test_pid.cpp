// Unit tests for the position PID. Run with: pio test -e native
#include <unity.h>

#include "pid.h"

namespace {

constexpr float kDt = 0.001f;

// A trivial first-order plant: output moves toward the command at a fixed
// rate. Enough to show the controller converges and does not run away.
float simulate(Pid& pid, float setpoint, float start, int steps) {
    float position = start;
    for (int i = 0; i < steps; ++i) {
        const float u = pid.update(setpoint, position, kDt);
        position += u * kDt * 10.0f;
    }
    return position;
}

}  // namespace

void setUp(void) {}
void tearDown(void) {}

void test_proportional_only_moves_toward_setpoint(void) {
    Pid pid(2.0f, 0.0f, 0.0f, 1.0f);
    pid.reset(0.0f);

    const float end = simulate(pid, 1.0f, 0.0f, 500);

    TEST_ASSERT_TRUE(end > 0.0f);
    TEST_ASSERT_TRUE(end <= 1.05f);
}

void test_integral_removes_steady_state_error(void) {
    Pid pid(1.0f, 20.0f, 0.0f, 1.0f);
    pid.reset(0.0f);

    const float end = simulate(pid, 0.5f, 0.0f, 4000);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, end);
}

void test_output_respects_limit(void) {
    Pid pid(1000.0f, 0.0f, 0.0f, 0.4f);
    pid.reset(0.0f);

    const float u = pid.update(10.0f, 0.0f, kDt);

    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.4f, u);
}

void test_negative_error_gives_negative_output(void) {
    Pid pid(1.0f, 0.0f, 0.0f, 1.0f);
    pid.reset(1.0f);

    const float u = pid.update(0.0f, 1.0f, kDt);

    TEST_ASSERT_TRUE(u < 0.0f);
}

void test_integral_does_not_wind_up_while_saturated(void) {
    Pid pid(1.0f, 50.0f, 0.0f, 0.2f);
    pid.reset(0.0f);

    // Hold a large error against a tight output limit. Conditional
    // integration should stop the integral growing without bound.
    for (int i = 0; i < 2000; ++i) {
        pid.update(100.0f, 0.0f, kDt);
    }

    TEST_ASSERT_TRUE(pid.integral() < 1.0f);
}

void test_derivative_on_measurement_has_no_setpoint_kick(void) {
    Pid pid(1.0f, 0.0f, 5.0f, 100.0f);
    pid.reset(0.0f);

    // Settle at zero, then step the setpoint with the measurement unchanged.
    pid.update(0.0f, 0.0f, kDt);
    const float u = pid.update(1.0f, 0.0f, kDt);

    // Output is the proportional term alone: a derivative on the error would
    // have produced a spike of kd * 1.0 / dt here.
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, u);
}

void test_reset_clears_integral(void) {
    Pid pid(0.0f, 10.0f, 0.0f, 10.0f);
    pid.reset(0.0f);

    for (int i = 0; i < 100; ++i) pid.update(1.0f, 0.0f, kDt);
    TEST_ASSERT_TRUE(pid.integral() > 0.0f);

    pid.reset(0.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-9f, 0.0f, pid.integral());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_proportional_only_moves_toward_setpoint);
    RUN_TEST(test_integral_removes_steady_state_error);
    RUN_TEST(test_output_respects_limit);
    RUN_TEST(test_negative_error_gives_negative_output);
    RUN_TEST(test_integral_does_not_wind_up_while_saturated);
    RUN_TEST(test_derivative_on_measurement_has_no_setpoint_kick);
    RUN_TEST(test_reset_clears_integral);
    return UNITY_END();
}
