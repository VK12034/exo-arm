// Planar two-link kinematics and gravity compensation for the arm.
//
// Convention: the arm is treated as a two-link planar chain moving in the
// sagittal plane. q0 is shoulder angle measured from horizontal-forward,
// positive raising the upper arm. q1 is elbow angle relative to the upper
// arm, positive flexing. Both are radians, both zero when the arm hangs
// straight out in front of the wearer.
//
// Header-only and free of Arduino dependencies so the native unit tests can
// exercise it directly.
#pragma once

#include <math.h>

namespace kin {

constexpr float kGravity = 9.80665f;

struct ArmParams {
    float link_length[2];  // m, proximal joint to distal joint
    float link_mass[2];    // kg, link plus its share of actuator and cuff
    float link_com[2];     // m, proximal joint to that link's centre of mass
};

struct Vec2 {
    float x;
    float y;
};

// Position of the wrist cuff relative to the shoulder joint, metres.
inline Vec2 forward(const ArmParams& p, float q0, float q1) {
    const float a = q0;
    const float b = q0 + q1;
    return {
        p.link_length[0] * cosf(a) + p.link_length[1] * cosf(b),
        p.link_length[0] * sinf(a) + p.link_length[1] * sinf(b),
    };
}

// Jacobian of the wrist position with respect to the joint angles, row-major
// as { dx/dq0, dx/dq1, dy/dq0, dy/dq1 }.
inline void jacobian(const ArmParams& p, float q0, float q1, float j[4]) {
    const float a = q0;
    const float b = q0 + q1;
    const float l0 = p.link_length[0];
    const float l1 = p.link_length[1];

    j[0] = -l0 * sinf(a) - l1 * sinf(b);
    j[1] = -l1 * sinf(b);
    j[2] =  l0 * cosf(a) + l1 * cosf(b);
    j[3] =  l1 * cosf(b);
}

// Maps a force applied at the cuff (newtons, in the sagittal plane) to the
// joint torques it produces, via the Jacobian transpose.
inline void forceToJointTorque(const ArmParams& p, float q0, float q1,
                               Vec2 force, float tau[2]) {
    float j[4];
    jacobian(p, q0, q1, j);
    tau[0] = j[0] * force.x + j[2] * force.y;
    tau[1] = j[1] * force.x + j[3] * force.y;
}

// Joint torques needed to hold the arm against gravity at the given pose.
// This is fed forward so the PID only has to correct the residual, which
// keeps the gains low enough to stay comfortable to wear.
inline void gravityTorque(const ArmParams& p, float q0, float q1, float tau[2]) {
    const float a = q0;
    const float b = q0 + q1;

    const float m0 = p.link_mass[0];
    const float m1 = p.link_mass[1];

    tau[0] = (m0 * p.link_com[0] + m1 * p.link_length[0]) * kGravity * cosf(a)
             + m1 * p.link_com[1] * kGravity * cosf(b);
    tau[1] = m1 * p.link_com[1] * kGravity * cosf(b);
}

// Inverse kinematics for a target cuff position. Returns false when the
// target is outside the reachable annulus, in which case the outputs are
// left untouched. elbow_up selects between the two mirrored solutions.
inline bool inverse(const ArmParams& p, Vec2 target, bool elbow_up,
                    float* q0, float* q1) {
    const float l0 = p.link_length[0];
    const float l1 = p.link_length[1];

    const float r2 = target.x * target.x + target.y * target.y;
    const float r  = sqrtf(r2);

    // Reject targets outside the annulus, with a small tolerance so that a
    // point exactly on either boundary still solves.
    constexpr float kTol = 1e-4f;
    if (r > l0 + l1 + kTol) return false;
    if (r < fabsf(l0 - l1) - kTol) return false;

    float cos_q1 = (r2 - l0 * l0 - l1 * l1) / (2.0f * l0 * l1);
    if (cos_q1 > 1.0f)  cos_q1 = 1.0f;
    if (cos_q1 < -1.0f) cos_q1 = -1.0f;

    const float q1_mag = acosf(cos_q1);
    const float q1_sol = elbow_up ? q1_mag : -q1_mag;

    const float q0_sol = atan2f(target.y, target.x)
                         - atan2f(l1 * sinf(q1_sol), l0 + l1 * cosf(q1_sol));

    *q0 = q0_sol;
    *q1 = q1_sol;
    return true;
}

}  // namespace kin
