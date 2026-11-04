# Commissioning and tuning

Do all of this with the arm **on the bench, not worn**, until the last step.

## 1. Check encoder direction and scale

    exoarm --port COM5 status

Backdrive each joint by hand through a known angle. If the reported angle
moves the wrong way, swap that encoder A and B pins. Do not negate it in
software, or the limit logic ends up fighting the sign.

If the magnitude is wrong, kGearRatio or kEncoderCpr is wrong. Measure over a
large angle: move 90 degrees by protractor and compare.

## 2. Check motor direction

In HOLD, nudge the target a few degrees:

    exoarm --port COM5 mode hold
    exoarm --port COM5 move --joint 0 --deg 5

The joint must move toward the target. If it runs away, the motor leads are
reversed relative to the encoder. Swap the motor leads.

**This is the failure that hurts someone.** A sign error turns the position
loop into positive feedback and the joint goes to a hard stop at full duty.
Verify it on the bench, at a current limit of 1 A, every time the wiring
changes.

## 3. Measure the gravity feed-forward scale

With gains at zero and the joint in HOLD, raise the duty by hand until the
joint just holds itself horizontal. Record that duty. The holding torque at
horizontal comes from the model:

    tau = (m0*lc0 + m1*l0)*g + m1*lc1*g      (shoulder)
    tau = m1*lc1*g                           (elbow)

kGravityFfDutyPerNm is then duty / tau. Repeat at a couple of angles and take
the mean. Expect 0.05 to 0.12 for the geometry in config.h.

Get this right before touching the PID. A good feed-forward term is what lets
the gains stay low, and low gains are what make the arm feel transparent
rather than stiff.

## 4. Tune the position loop

Per joint, starting from kp at half the value in config.h, ki and kd at zero:

1. Raise kp until a 10 degree step just starts to overshoot, then back off
   about 20 percent.
2. Raise kd until the overshoot is gone. Too much and you will hear the joint
   buzz at the velocity filter cutoff; back off until it is silent.
3. Raise ki until steady-state error at a held angle disappears within a
   second or two. This is the one to keep small. Integral on a worn device is
   what produces slow creep the wearer cannot explain.

    exoarm --port COM5 gains --joint 0 --kp 9 --ki 1.8 --kd 0.22
    exoarm --port COM5 record --seconds 10 --out step.csv

Record a step for each change rather than judging by feel. Gains that look
fine on the bench feel very different with an arm in the cuffs.

Write the final values into config.h. Runtime gains are for the tuning
session only and are lost at reset.

## 5. Tune the assist gain

In ASSIST, start at kDefaultAssistGain and adjust so that a comfortable push
produces a comfortable speed.

| Symptom | Fix |
|---|---|
| Arm drifts when it is just worn | Raise kForceDeadbandN |
| Feels heavy, you have to shove it | Raise the assist gain |
| Darts away, feels twitchy | Lower the assist gain, or lower kForceCutoffHz |
| Oscillates while you hold still | Lower the assist gain. You are inside the loop now, and you have dynamics too |

## 6. First wear

Follow the checklist in safety.md. Halve the gains again for the first
session.
