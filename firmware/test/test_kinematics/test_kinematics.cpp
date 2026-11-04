// Unit tests for the planar kinematics. Run with: pio test -e native
#include <unity.h>

#include "kinematics.h"

namespace {

constexpr kin::ArmParams kArm = {
    { 0.280f, 0.255f },   // link lengths, m
    { 0.840f, 0.520f },   // link masses, kg
    { 0.145f, 0.120f },   // centres of mass, m
};

constexpr float kPi = 3.14159265358979f;

}  // namespace

void setUp(void) {}
void tearDown(void) {}

void test_forward_fully_extended(void) {
    const kin::Vec2 p = kin::forward(kArm, 0.0f, 0.0f);

    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.535f, p.x);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, p.y);
}

void test_forward_shoulder_raised(void) {
    const kin::Vec2 p = kin::forward(kArm, kPi / 2.0f, 0.0f);

    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, p.x);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.535f, p.y);
}

void test_forward_elbow_folded(void) {
    // Elbow folded all the way back onto the upper arm: the cuff sits at the
    // difference of the link lengths, in front of the shoulder.
    const kin::Vec2 p = kin::forward(kArm, 0.0f, kPi);

    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.025f, p.x);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, p.y);
}

void test_inverse_round_trips(void) {
    const float q0 = 0.6f;
    const float q1 = 0.9f;

    const kin::Vec2 p = kin::forward(kArm, q0, q1);

    float s0 = 0.0f;
    float s1 = 0.0f;
    TEST_ASSERT_TRUE(kin::inverse(kArm, p, true, &s0, &s1));

    TEST_ASSERT_FLOAT_WITHIN(1e-4f, q0, s0);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, q1, s1);
}

void test_inverse_rejects_out_of_reach(void) {
    float q0 = 99.0f;
    float q1 = 99.0f;

    const kin::Vec2 too_far = { 1.5f, 0.0f };
    TEST_ASSERT_FALSE(kin::inverse(kArm, too_far, true, &q0, &q1));

    // Outputs must be left untouched on failure.
    TEST_ASSERT_FLOAT_WITHIN(1e-9f, 99.0f, q0);
    TEST_ASSERT_FLOAT_WITHIN(1e-9f, 99.0f, q1);
}

void test_inverse_accepts_boundary(void) {
    float q0 = 0.0f;
    float q1 = 0.0f;

    const kin::Vec2 at_reach = { 0.535f, 0.0f };
    TEST_ASSERT_TRUE(kin::inverse(kArm, at_reach, true, &q0, &q1));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.0f, q1);
}

void test_gravity_torque_is_maximum_when_horizontal(void) {
    float tau_flat[2];
    float tau_up[2];

    kin::gravityTorque(kArm, 0.0f, 0.0f, tau_flat);
    kin::gravityTorque(kArm, kPi / 2.0f, 0.0f, tau_up);

    // Arm straight out: both joints carry their full load.
    const float expected_shoulder =
        (kArm.link_mass[0] * kArm.link_com[0] + kArm.link_mass[1] * kArm.link_length[0])
        * kin::kGravity
        + kArm.link_mass[1] * kArm.link_com[1] * kin::kGravity;

    TEST_ASSERT_FLOAT_WITHIN(1e-4f, expected_shoulder, tau_flat[0]);
    TEST_ASSERT_TRUE(tau_flat[0] > 0.0f);

    // Arm pointing straight up: gravity acts along the links, so neither
    // joint has to hold any moment.
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, tau_up[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, tau_up[1]);
}

void test_elbow_gravity_is_independent_of_shoulder_lever(void) {
    float tau[2];
    kin::gravityTorque(kArm, 0.0f, 0.0f, tau);

    const float expected_elbow =
        kArm.link_mass[1] * kArm.link_com[1] * kin::kGravity;

    TEST_ASSERT_FLOAT_WITHIN(1e-4f, expected_elbow, tau[1]);
    TEST_ASSERT_TRUE(tau[0] > tau[1]);
}

void test_downward_force_at_cuff_loads_both_joints(void) {
    float tau[2];
    const kin::Vec2 push_down = { 0.0f, -20.0f };

    kin::forceToJointTorque(kArm, 0.0f, 0.0f, push_down, tau);

    // Pushing down on an arm held straight out tries to rotate both joints
    // the same way, and the shoulder sees the longer lever.
    TEST_ASSERT_TRUE(tau[0] < 0.0f);
    TEST_ASSERT_TRUE(tau[1] < 0.0f);
    TEST_ASSERT_TRUE(tau[0] < tau[1]);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_forward_fully_extended);
    RUN_TEST(test_forward_shoulder_raised);
    RUN_TEST(test_forward_elbow_folded);
    RUN_TEST(test_inverse_round_trips);
    RUN_TEST(test_inverse_rejects_out_of_reach);
    RUN_TEST(test_inverse_accepts_boundary);
    RUN_TEST(test_gravity_torque_is_maximum_when_horizontal);
    RUN_TEST(test_elbow_gravity_is_independent_of_shoulder_lever);
    RUN_TEST(test_downward_force_at_cuff_loads_both_joints);
    return UNITY_END();
}
