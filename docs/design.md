# Design

## What this is

A two-degree-of-freedom powered exoskeleton for one arm. It supports the
shoulder in flexion/extension and the elbow in flexion/extension, in the
sagittal plane only. The wearer drives it; the arm follows and carries the
weight.

It is a workshop build, not a product. See [safety.md](safety.md) before
anyone puts it on.

## Requirements

| # | Requirement | Why |
|---|---|---|
| R1 | Hold the wearer's arm against gravity with no sustained effort from them | The whole point of the device |
| R2 | Follow the wearer's intent with no perceptible fight | A device that resists is worse than no device |
| R3 | Remove power within 50 ms of any interlock opening | A powered joint on a limb must fail safe fast |
| R4 | Never command a joint beyond its mechanical range | Hard stops are a last resort, not a control strategy |
| R5 | Be removable by the wearer alone, unpowered | They must never be trapped in it |
| R6 | Log everything at 100 Hz for post-run analysis | You cannot tune what you cannot see |

## Degrees of freedom

| Joint | Motion | Range (soft limits) |
|---|---|---|
| J0 shoulder | Flexion / extension | -20° to 149° |
| J1 elbow | Flexion / extension | 0° to 135° |

Shoulder abduction, rotation and the wrist are unpowered. Keeping the powered
chain planar means the kinematics stay two-link and the gravity model stays
closed-form, which is what makes the feed-forward in
[`kinematics.h`](../firmware/include/kinematics.h) worth having.

## Architecture

```
      wearer
        │ interaction force (load cells at the cuffs)
        ▼
┌───────────────────┐   1 kHz    ┌──────────────┐
│ control loop      │──────────▶ │ motor driver │──▶ geared DC motor
│  · gravity FF     │            └──────────────┘        │
│  · position PID   │◀───────────────────────────────────┘
│  · limit shaping  │   encoder, current sense
└───────────────────┘
        │ 100 Hz telemetry / commands
        ▼
┌───────────────────┐
│ host tools        │  plots, logging, gain tuning
└───────────────────┘
```

The Teensy owns everything safety-related. The host can ask for modes,
targets and gains, but it cannot override an interlock, and the arm stops if
the host goes quiet for `kHostTimeoutMs`. A laptop crashing must not be able
to leave a motor driving.

## Control

Each joint runs the same inner loop regardless of mode:

1. **Gravity feed-forward.** The two-link model gives the holding torque at
   the current pose, converted to a duty term. This does most of the work, so
   the PID only corrects the residual and its gains can stay low — which is
   what keeps the arm feeling transparent rather than stiff.
2. **Position PID**, derivative on measurement, conditional integration.
3. **Limit shaping.** Command is scaled down inside `kLimitMarginRad` of a
   soft limit, in the direction of the limit only.
4. **Velocity and current ceilings**, then a slew limiter on duty.

### Modes

| Mode | Target comes from | Use |
|---|---|---|
| `IDLE` | — | Drivers asleep. Default at power-on and after any fault |
| `HOLD` | Frozen at the pose on entry | Static support, bench testing |
| `POSITION` | Host | Scripted motion, step responses during tuning |
| `ASSIST` | Interaction force at the cuffs | Normal wear |

`ASSIST` is admittance control: measured force above a deadband becomes a
velocity command, which is integrated into the position target. Push and the
arm moves with you; stop pushing and it holds. The deadband is what stops it
drifting from sensor noise while it is simply being worn.

## Why these choices

**Teensy 4.1 rather than an SBC.** A 1 kHz loop that must not jitter does not
belong on a general-purpose OS. The Teensy has hardware quadrature-capable
pins, enough PWM, and a predictable interrupt latency.

**Brushed gearmotors rather than BLDC.** Simpler drivers, no commutation to
get wrong, and backdrivability is handled by the gear ratio choice rather
than by field-oriented control. The cost is efficiency and noise, neither of
which matters here.

**Load cells rather than EMG.** EMG is tempting and gives earlier intent, but
it needs per-session calibration, skin prep and filtering that is its own
project. Force at the cuff is unambiguous and works the moment it is bolted
on. EMG remains a sensible later addition on top of the same admittance loop.

**Incremental encoders with a parked-pose reference.** Absolute encoders
would remove the startup ritual, but incremental encoders plus a hard stop to
park against costs nothing and is one fewer part to source. The trade is that
power-on position is only as good as the parking.

## Open questions

- Gravity feed-forward is currently scaled per joint by a measured constant.
  A proper motor model (back-EMF, winding resistance) would make the term
  supply-voltage independent, which matters once it runs off a battery.
- No forearm pronation support. Adding it breaks the planar assumption and
  pushes the gravity model to full Newton-Euler.
- The limit shaping ramps the command, not the velocity. A joint moving fast
  into a limit still relies on the mechanical stop.
