// Exoskeleton arm firmware entry point.
//
// The control loop runs from a hardware timer at cfg::kControlHz so its
// timing does not depend on how long the serial work in loop() takes. The
// main loop owns everything that talks to the host.
#include <Arduino.h>

#include "config.h"
#include "joint.h"
#include "kinematics.h"
#include "protocol.h"
#include "safety.h"
#include "telemetry.h"

namespace {

Joint g_joints[cfg::kJointCount] = { Joint(0), Joint(1) };

Safety    g_safety;
Telemetry g_link;

IntervalTimer g_control_timer;

// Written by the host over the link, read by the control loop.
volatile float g_assist_gain = cfg::kDefaultAssistGain;

constexpr kin::ArmParams kArm = {
    { cfg::kLinkLength[0], cfg::kLinkLength[1] },
    { cfg::kLinkMass[0],   cfg::kLinkMass[1]   },
    { cfg::kLinkCom[0],    cfg::kLinkCom[1]    },
};

// Turns the measured interaction force at a joint into a velocity command.
// Below the deadband the wearer is treated as not pushing, which keeps the
// arm still when it is just being worn.
float assistVelocity(float force_n, float gain) {
    if (force_n > cfg::kForceDeadbandN) {
        return (force_n - cfg::kForceDeadbandN) * gain;
    }
    if (force_n < -cfg::kForceDeadbandN) {
        return (force_n + cfg::kForceDeadbandN) * gain;
    }
    return 0.0f;
}

void controlStep() {
    const uint32_t now_ms = millis();

    const bool safe = g_safety.check(g_joints, cfg::kJointCount, now_ms);

    float tau[cfg::kJointCount] = { 0.0f, 0.0f };
    kin::gravityTorque(kArm, g_joints[0].position(), g_joints[1].position(), tau);

    if (safe) {
        const proto::Mode mode = g_safety.mode();

        for (uint8_t i = 0; i < cfg::kJointCount; ++i) {
            if (!g_joints[i].enabled()) g_joints[i].enable();

            if (mode == proto::Mode::kAssist) {
                g_joints[i].setVelocityTarget(
                    assistVelocity(g_joints[i].force(), g_assist_gain));
            } else if (mode == proto::Mode::kHold) {
                g_joints[i].setVelocityTarget(0.0f);
            }
            // kPosition leaves the target where the host last set it.
        }
    }

    for (uint8_t i = 0; i < cfg::kJointCount; ++i) {
        g_joints[i].update(cfg::kDt, tau[i]);
    }
}

}  // namespace

void setup() {
    g_link.begin(921600);
    g_link.bindAssistGain(&g_assist_gain);

    g_safety.begin();

    for (uint8_t i = 0; i < cfg::kJointCount; ++i) {
        g_joints[i].begin();

        // No absolute encoders: the arm is parked against its lower hard
        // stop at power-on and that pose defines the reference. See
        // docs/operation.md before changing this.
        g_joints[i].zeroAt(cfg::kMinAngle[i]);
    }

    g_control_timer.begin(controlStep, 1000000UL / cfg::kControlHz);
    g_control_timer.priority(64);  // above the USB stack, below nothing else

    g_link.sendLog("exo-arm ready; park at lower stop before enabling");
}

void loop() {
    static uint32_t next_telemetry_ms = 0;

    const uint32_t now_ms = millis();

    g_link.poll(g_joints, cfg::kJointCount, &g_safety, now_ms);

    if (static_cast<int32_t>(now_ms - next_telemetry_ms) >= 0) {
        next_telemetry_ms = now_ms + (1000UL / cfg::kTelemetryHz);
        g_link.sendTelemetry(g_joints, cfg::kJointCount, g_safety, now_ms);
    }
}
