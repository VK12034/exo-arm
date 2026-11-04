# Design

## What this is

A powered elbow for the right arm. The wearer presses UP or DOWN on a hand
pendant and the forearm raises or lowers, carrying its own weight and theirs.
The shoulder is not actuated; a sling and waist belt carry the reaction.

It is a workshop build. See [safety.md](safety.md) before anyone puts it on.

## Requirements

| # | Requirement | Why |
|---|---|---|
| R1 | Hold the forearm at any angle with no power applied | A limb support that fails to a drop is not a support |
| R2 | Move only while a button is held | Intent must be continuous and unambiguous |
| R3 | Remove motor power within one loop of the e-stop opening | A powered joint on a limb must fail safe fast |
| R4 | Never drive outside the elbow's working range | Hard stops are a last resort, not a strategy |
| R5 | Be removable by the wearer alone, unpowered | They must never be trapped in it |
| R6 | Be audible and slow enough to be interrupted | The wearer needs time to react and reach the e-stop |

## Why these choices

**One degree of freedom.** The elbow is where the leverage problem is: a
forearm held out at 90° is most of the static load on the arm, and it is the
motion that fatigues first. Powering the shoulder as well would double the
actuators, double the failure modes, and need a frame stiff enough to react
shoulder torque into the torso. The sling does that job passively.

**A worm gearbox, and specifically 15:1.** A worm at this ratio is
self-locking: the output cannot drive the input. The elbow therefore holds
its angle with the motor off. That single property removes the entire
control problem that an exoskeleton usually has — no gravity model, no
holding current, no PID fighting the weight of the arm, no thermal budget for
standing still. It costs backdrivability and efficiency, neither of which
this machine needs.

**A stepper rather than a servo.** With the load self-locking, position is
whatever has been stepped since power-on, and a missed step is a slow drift
rather than a runaway. A closed loop would buy accuracy the application does
not need, at the price of an encoder, its cable across a moving joint, and
the failure modes of both.

**An Arduino Uno.** There is no control loop to run. The firmware debounces
three inputs, ramps a velocity, and counts steps. An 8-bit micro at 16 MHz is
not the constraint; a part that can be replaced from any electronics shop is
worth more here than headroom.

**A pendant rather than intent sensing.** Load cells or EMG would let the arm
follow the wearer, but a non-backdrivable joint cannot be led — the wearer
would be pushing against a worm that does not yield. Buttons are honest about
what the machine can do.

## Architecture

```
  pendant                     control box, waist belt
  ┌─────────┐   6-core        ┌──────────────────────┐        ┌──────────┐
  │ UP      │ ─────────────── │ Arduino Uno          │ step   │ TB6600   │
  │ DOWN    │                 │  · debounce          │ ─────▶ │  driver  │
  │ E-STOP  │ ══ NC loop ══╗  │  · velocity planner  │ dir    └────┬─────┘
  └─────────┘              ║  │  · step generator    │ enable      │
                           ║  └──────────────────────┘             ▼
                           ║                                 NEMA 23 + worm
                           ╚═════ breaks driver supply ═══════════ 15:1
                                                                   │
                                                                   ▼
                                                            elbow, direct
```

The e-stop appears twice on purpose: the firmware reads it and stops, and the
loop also breaks the driver's supply in hardware. The motor dies whether or
not the Arduino is behaving.

## Firmware

Three layers, each able to stop the arm without the one above it:

1. **Pendant.** Debounced at 25 ms. Both buttons, or neither, means hold —
   an ambiguous input is never resolved by guessing.
2. **Planner.** Turns a button into a velocity that ramps at 45°/s² up to
   20°/s, and begins decelerating when the remaining travel equals the
   stopping distance, so the arm comes to rest at the limit instead of
   arriving at it under power.
3. **Step generator.** A 10 kHz timer interrupt accumulates a 16.16
   fixed-point increment and emits a pulse on each rollover. It holds its own
   copy of the travel limits and refuses to pulse past them whatever the
   planner asks for.

Direction changes are applied only with the generator stopped, because the
planner always passes through zero velocity on a reversal. A pulse emitted
against a stale direction line would put the step counter out of step with
the real arm, and the counter is the only position reference there is.

### Position, and the lack of an encoder

There is no absolute reference. Position is steps counted from the pose the
arm was in at power-on, which the firmware takes to be full extension. Park
the arm against its extension stop before switching on; that ritual is the
reference. Holding both buttons for three seconds re-zeroes at the current
pose, which is there because accumulated step loss has nothing else to
correct it — and which also defeats the soft limits, so it is deliberately
awkward to trigger.

### Idle behaviour

After three seconds without motion the driver is de-energised. The worm holds
the elbow, so the only things a holding current would buy are heat in the
motor, heat in the driver, and noise the wearer has to carry.

## Numbers

From [cad/README.md](../cad/README.md), measured off the Fusion model:

| Quantity | Value |
|---|---|
| Elbow to forearm cuff | 193 mm |
| Shoulder to elbow | 318 mm |
| Forearm module mass | 0.214 kg |
| Static load at the elbow | ≈ 2.8 N·m with a typical forearm |
| Available output torque | ≈ 20 N·m after worm losses |

The margin is roughly sevenfold. That is not over-engineering: worm
efficiency falls with wear and temperature, the load rises if the wearer
carries anything, and a stepper that stalls loses position silently.

## Open questions

- No limit switches. The soft limits are counted steps, so they are only as
  good as the power-on park and the absence of step loss. A single switch at
  full extension would make homing real.
- Microstepping is set by DIP switches on the TB6600 and assumed to be 1/8 in
  `config.h`. Nothing checks that the two agree; if they do not, every angle
  is wrong by the same factor.
- Load monitoring infers rather than measures. Supply current tracks
  mechanical power, so a bind shows as a sustained rise and a lost rotor as a
  collapse, but a slow bind below the threshold is invisible and a collapse
  is indistinguishable from an unplugged motor. A shaft encoder would make
  this exact, and would also make homing real.
- The pendant cable crosses no joint, but the motor cable does. That is the
  cable that will eventually fail.
