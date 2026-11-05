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

Position is steps counted, with no encoder to check them against. What the
counter is referenced to is a lever switch at the extension end of travel.
Reaching it sets zero; until it is reached the firmware treats the count as
an assumption and holds the arm to 90° at 5°/s, so a parking error costs
range rather than putting the elbow somewhere it should not be.

The switch is deliberately not homed to on power-up. The one rule this
machine has is that it moves only while a human holds a button, and driving
the arm at a switch by itself to fix a lesser problem would break it. So the
zero is adopted during the extension the wearer was commanding anyway. The
cost is a session that starts restricted; the alternative was a machine that
moves on its own.

Where the switch and the counter disagree, the switch wins. It is a physical
feature of the machine and the counter is arithmetic that drifts with every
lost step. A small disagreement is corrected silently, because the count
rounds. Past five degrees it faults instead, because that much loss means the
flexion limit moved too while the arm was in use, and correcting only the
zero would hide that.

This is one-ended in two senses. Zero is a switch and 135° is still
arithmetic from it; and the drift measurement only works in the direction
that matters. Lost steps leave the arm more extended than it claims, so it
arrives at the switch with the count reading positive and the error is
caught. The opposite error never reaches the switch, because the soft minimum
stops the arm at a reported 0° first. That is the harmless direction, but it
does mean a clean homing message is not a proof of correctness.

The re-zero gesture survives as a fallback for a build with no switch fitted,
and is recorded as asserted rather than measured so the status output can say
which of the two the current zero is. It sets the soft minimum at the
asserted pose, which is what a switchless build needs and which also means a
wrong assertion cannot be walked back without a power cycle. Keeping the soft
minimum was worth more than that recovery path.

### The power-on log

Two things used to vanish at the wrong moment. A fault reason, because the
wearer switches off to get the arm away from themselves and the LED stops
blinking. And the homing drift figure, which only means anything as a trend
and which commissioning used to ask you to keep on paper.

Both now go to EEPROM: the last 16 events, each tagged with a session number,
printed at power-on.

Printed at power-on and nowhere else, which is the part worth explaining. The
obvious interface is a serial command to dump the log, and that was rejected.
This firmware accepts no serial input at all, and that is a property rather
than an omission: the serial port is output only, so there is no parser, no
command table, and no path into the machine that is not a button. Reading a
log is harmless; the code that would read the request for it is what
shouldn't exist. Printing unconditionally at boot costs nothing and keeps
that property intact.

Writes are rare by construction, which is what makes EEPROM reasonable on a
part rated for 100k cycles per cell. A drift record needs a trip to the home
switch and a correction worth reporting. A fault record needs a fault.
Routine e-stop presses are deliberately not recorded: normal operation, and
logging them would both crowd the ring and spend the write budget. Every byte
goes through update semantics, so a byte already holding the right value is
not rewritten.

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

- The mechanical hard stops are not designed. Both this document and
  [safety.md](safety.md) describe them as the layer behind the soft limits,
  and the home switch is specified relative to one, but the Fusion model
  contains no travel stop and no switch mount. The firmware layers are real
  and tested; the physical one is an intention. It is the largest
  inconsistency between the documentation and the design.
- The 135° soft limit itself has no physical reference. Both ends of travel
  now carry a switch, but neither is at the limit: the home switch defines
  zero, and the overrun switch sits three degrees past the flexion limit as a
  check on the firmware. Between them the limit is still arithmetic.
- The overrun switch cannot be distinguished from a working one that has
  never fired. It is the one component here whose failure is invisible in
  normal use.
- The home switch is a single contact with nothing to cross-check it. The
  normally-open wiring means a break costs range rather than inventing a
  zero, and a contact stuck closed far into flexion faults, but a lever bent
  so that it closes early would be believed.
- Microstepping is set by DIP switches on the TB6600 and assumed to be 1/8 in
  `config.h`. The home-to-overrun span now checks this, because it is the one
  figure that compares a step count against a physical distance: a driver set
  to 1/16 against a firmware expecting 1/8 halves the measured span, which no
  tolerance absorbs. But the check only runs when the overrun switch is
  reached, which takes the deliberate procedure in step 7 of
  [commissioning.md](commissioning.md). It is a commissioning check, not a
  running one.
- Load monitoring infers rather than measures. Supply current tracks
  mechanical power, so a bind shows as a sustained rise and a lost rotor as a
  collapse, but a slow bind below the threshold is invisible and a collapse
  is indistinguishable from an unplugged motor. A shaft encoder would make
  this exact, and would reference position continuously rather than once per
  trip to the switch.
- The pendant cable crosses no joint, but the motor cable does. That is the
  cable that will eventually fail.
