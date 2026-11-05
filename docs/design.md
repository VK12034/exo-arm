# Design

## What this is

A powered elbow for the right arm. The wearer presses UP or DOWN on a hand
pendant and the forearm raises or lowers, carrying its own weight and theirs.
The shoulder is not actuated; a sling and waist belt carry the reaction.

It is a workshop build. See [safety.md](safety.md) before anyone puts it on.

## Requirements

| # | Requirement | Why |
|---|---|---|
| R1 | Hold the forearm at any angle with no power applied. **Not yet shown to be met** | A limb support that fails to a drop is not a support |
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

**A worm gearbox with a chain stage.** The NMRV030 at 30:1 lies along the upper
arm, and a #35 chain from a 15T to a 30T sprocket carries the drive to the elbow,
for 60:1 overall. That moves the motor's mass up the arm, away from the elbow,
and keeps it from standing far off the arm.

The intent is for the worm to hold the arm with the motor off. **The manufacturer's
figures say it should:** Motovario lists the 030 at 30:1 with a static efficiency of
0.39, below the 0.5 it requires. A 15:1 worm, at 0.55, does not meet that, which is why
this one replaced it. Until the holding test passes, treat this as a machine that needs
holding current and can move when it loses power. A chain failure or a skipped
tooth would also release the forearm whatever the worm does.

**A deliberately small motor.** The arm needs about 0.13 N·m from the motor to carry the
static load, so 0.55 N·m leaves margin without much more force than the arm needs.
A stronger motor with its current turned down depends on the DIP switches staying
right. This one limits the force even if they are wrong, though not to a level that is
safe for an arm in every case. See [loads.md](loads.md).

**A stepper rather than a servo.** Position is whatever has been stepped since
the last homing, and a missed step is a drift rather than a runaway. Control
stays open-loop, because the application does not need closed-loop accuracy. An
encoder supervises the count instead, so its failure costs a check rather than
control of the arm.

**An Arduino Uno.** There is no control loop to run. The firmware debounces
three inputs, ramps a velocity, and counts steps. An 8-bit micro at 16 MHz is
not the constraint; a part that can be replaced from any electronics shop is
worth more here than headroom.

**A pendant rather than intent sensing.** Load cells or EMG would let the arm
follow the wearer, but a geared drive this stiff cannot be led by pushing on
it, whether or not it turns out to be self-locking. Buttons are honest about
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
                           ╚═════ breaks driver supply ═══════════ 30:1
                                                                   │
                                                                   ▼
                                                            #35 chain, 2:1
                                                                   │
                                                                   ▼
                                                            elbow        
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

### Position

Position is steps counted. What the counter is referenced to is a lever switch at the extension end of travel.
Reaching it sets zero; until it is reached the firmware treats the count as
an assumption and holds the arm to 60° at 5°/s, so a parking error costs
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

This is one-ended in two senses. Zero is a switch and 88° is still
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

### The encoder

An AS5600 on the elbow axis measures the joint itself, so worm backlash, chain slip,
lost motor steps and back-driving all appear in one number. It needs no calibration
constant: it takes its zero when the arm homes, and only the difference from the
step count matters afterwards.

It is read at 50 Hz. A disagreement above 3° for 400 ms faults the arm and drops it to
restricted travel until the switch re-references the count. That is checked while the
arm is stopped too, so a drivetrain giving way is caught rather than only noticed by
the wearer. After an e-stop, a forearm that moved is caught on the first check after
the arm is released.

A lost sensor faults once and the arm then carries on step-counted, because leaving
someone unable to move the arm is its own hazard. Both are logged. The 3° figure is a
placeholder until chain slack and worm backlash are measured.

### Faults and motor current

Only the e-stop removes motor current. Other faults stop the stepping and leave the
motor energised, because a firmware fault that cut the current could drop the forearm.
The cost is that an overload fault leaves holding torque on whatever the arm is
pushing against, until someone presses the e-stop. That trade is worth revisiting if
the holding test passes.

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

The motor stays energised when idle, which costs heat in the motor and driver and
some noise. `kDeenergiseWhenIdle` in `config.h` drops the current after three seconds
and also lets faults cut it. It stays off until the holding test in
[commissioning.md](commissioning.md) has passed under load on the real gearbox.

## Numbers

From [cad/README.md](../cad/README.md), measured off the Fusion model:

| Quantity | Value |
|---|---|
| Elbow to forearm cuff | 193 mm |
| Shoulder to elbow | 318 mm |
| Forearm module mass | 0.430 kg |
| Static load at the elbow | About 2.9 N·m, calculated for a 70 kg wearer |
| Output torque at full motor current | 12 to 21 N·m, from catalogue efficiencies |

That is 4 to 7 times the static load. The margin helps with a carried load and with
wear, but it is also the force that could reach an arm or a hard stop: see
[loads.md](loads.md).

It is also the figure that cannot be reduced in software, and the reason is
worth recording where the numbers are. A TB6600 regulates phase current to
its DIP setting, and stepper torque follows phase current, so that torque
is available at every commanded speed. Torque in fact rises as speed falls,
so commanding the arm slowly does not make it push more gently. There is no
firmware state between stepping and stopped, which makes the overload monitor
a trip rather than a limit: it notices that too much force is already going
in and cuts the drive.

The levers that do exist are the choice of motor, which sets the ceiling, and the DIP
current setting, which scales torque below it. Dropping the setting toward two or three
times the static load keeps margin for worm losses and a carried load while lowering
the worst case, at the cost of a smaller stall margin and therefore more lost steps.
That trade is a bench measurement, not a decision to be made from CAD. A mechanical
torque limiter on the output is the only fix that holds when the firmware is wrong.

## Open questions

- Does the drivetrain hold a loaded arm without current? The catalogue says it should,
  and the real gearbox has not been tested. If it does not, it needs a motor brake.
- There is no torque limiter. A friction clutch at the 30T sprocket is the likely
  form. See [loads.md](loads.md).
- The 88° soft limit itself has no physical reference. Both ends of travel carry
  a switch, but neither is at the limit: the home switch defines zero, and the
  overrun switch sits three degrees past the flexion limit as a check on the
  firmware. Between them the limit is still arithmetic.
- The chain guard's stiffness on two pillars is untested, and the encoder gap depends
  on it.
- The overrun switch cannot be distinguished from a working one that has
  never fired. It is the one component here whose failure is invisible in
  normal use.
- The home switch is a single contact with nothing to cross-check it. The
  normally-open wiring means a break costs range rather than inventing a
  zero, and a contact stuck closed far into flexion faults, but a lever bent
  so that it closes early would be believed.
- Microstepping is set by DIP switches on the TB6600 and assumed to be 1/4 in
  `config.h`. The home-to-overrun span now checks this, because it is the one
  figure that compares a step count against a physical distance: a driver set
  to 1/8 against a firmware expecting 1/4 halves the measured span, which no
  tolerance absorbs. But the check only runs when the overrun switch is
  reached, which takes the deliberate procedure in step 7 of
  [commissioning.md](commissioning.md). It is a commissioning check, not a
  running one.
- Load monitoring infers rather than measures. Supply current tracks mechanical
  power, so a bind shows as a sustained rise and a lost rotor as a collapse, but a slow
  bind below the threshold is invisible. The encoder covers the position side of that,
  but not force.
- With the gearbox on the upper arm, neither the pendant cable nor the motor
  cable crosses the elbow. The motor cable does run from the belt to the moving
  upper arm, and that is the cable to expect to fail first.
