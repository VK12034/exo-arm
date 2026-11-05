# exo-arm

![BudgetExoArm](cad/BudgetExoArm.png)

A single-degree-of-freedom powered elbow worn on the right arm. A NEMA 23
stepper drives the elbow through a 15:1 worm and a 2:1 chain stage; a sling
and waist belt carry the shoulder; the wearer commands it with a hand pendant.

This repository holds the Fusion design, the firmware, and the documentation
needed to build, commission and operate it.

> **A workshop build, not a product.** It is not a medical device, it meets no
> standard, nobody qualified has assessed it, and it has never run against
> hardware. Read [docs/safety.md](docs/safety.md) before anyone puts it on.

## The machine

| | |
|---|---|
| Degrees of freedom | 1 — elbow flexion and extension |
| Actuator | NEMA 23 stepper, 1.9 N·m, through an NMRV030 worm at 15:1 |
| Final drive | #35 chain, 15T to 30T, for 30:1 overall |
| Output | About 30 to 43 N·m at full motor torque, against about 2.9 N·m of static load |
| Resolution | 48 000 microsteps per elbow revolution, 133.3 per degree |
| Travel | About 96° between the hard stops, from about 21° to 118° of true flexion |
| Control | Arduino Uno and a TB6600 driver, belt-mounted |
| Interface | UP / DOWN buttons and a latching NC e-stop on a hand pendant |
| Encoder | AS5600 on the elbow axis, checking the step count |
| Device mass | 4.160 kg |

The drivetrain is far stronger than the arm needs and probably cannot hold itself.
**By Motovario's catalogue figures, an NMRV030 at 15:1 is not self-locking.** The
firmware keeps the motor energised when idle and through every fault except the
e-stop, but pressing the e-stop or losing power may let the forearm move. See
[docs/loads.md](docs/loads.md).

## Layout

| Path | What is in it |
|---|---|
| `firmware/` | Arduino Uno firmware: motion planning, step generation, interlocks |
| `cad/` | The Fusion design, STEP and STL exports, and measured mass properties |
| `docs/` | Design, safety, loads, wiring, BOM, commissioning, operation |
| `tools/fusion/` | Read-only Fusion script that regenerates everything in `cad/` |
| `tools/stall_loads.py` | The stall load calculation behind `docs/loads.md` |

## Firmware

```bash
cd firmware
pio run -e uno              # build
pio run -e uno -t upload    # flash
pio device monitor          # status at 115200 baud
pio test -e native          # unit tests, on the host
```

Holding a button produces a velocity, not a step: the planner ramps up, holds
at 20°/s, and begins decelerating early enough to come to rest at the travel
limit rather than arriving at it under power. That matters more than it
sounds, because a stepper stopped dead at speed loses steps.

That counter is referenced to a lever switch at the extension stop. Reaching it
sets zero; until it is reached the arm is held to 60° at 5°/s, because the
count is an assumption about how the arm was parked and not worth more than
that. Where the switch and the counter disagree the switch wins, and past
five degrees of disagreement it faults rather than correcting, because that
much lost motion means the flexion limit moved too.

The switch is not homed to automatically. The arm moves only while a button
is held, and that rule was worth keeping more than a session that starts at
full range: the zero is adopted during an extension the wearer was commanding
anyway.

Four things are enforced below the planner: the step generator refuses to
pulse past the travel limits whatever it is told, the e-stop drops the driver
enable before anything else runs, both buttons at once reads as stop rather
than as a guess, and the supply current is watched while the arm drives so a
bind faults it out.

A second switch sits three degrees past the flexion limit, and it is not a
limit. Reaching it means the planner and the step clamp both failed, so it
faults. It also earns its keep when nothing is wrong: the step count at the
moment it closes measures the distance from the home switch, which is the
only figure on this machine that can be checked against a physical length,
and so the only available check on whether the driver's microstep switches
agree with `config.h`.

An AS5600 encoder on the elbow axis checks the step count once the arm has homed.
If the measured elbow and the count disagree for long enough, whether from lost
steps, chain slip or back-driving, the arm faults and drops to restricted travel
until it reaches the switch again. Control stays open-loop: the encoder is there to
tell the truth rather than to close a loop. The supply current monitor stays as a
separate check, and `docs/safety.md` is explicit about what it cannot catch.

Faults and homing corrections go to EEPROM and are printed at the next
power-on, because a fault reason is gone by the time the wearer has switched
off and the drift figure only means anything as a trend. There is no serial
command to read it: this firmware accepts no serial input at all, and keeping
that true was worth more than a nicer interface.

## Safety model

The e-stop loop is normally closed to ground. A pressed mushroom, an
unplugged pendant and a cut cable all read identically: open, meaning stop.
It breaks the driver supply in hardware as well as being read by the
firmware, so the motor dies whether or not the Arduino is behaving. Every other
fault stops the stepping and leaves the motor energised, because a fault that cut
the current could drop the forearm by itself.

At power-on the firmware starts in a fault state and will not leave it until
the e-stop reads closed **and** both buttons are released, so a stuck button
cannot drive the arm the moment power arrives.

The home switch is wired the opposite way round, normally open, and
deliberately so. For a stop command the safe reading of a broken wire is
"stop"; for a position reference it is "I do not know where I am". Wired
normally closed, a cut cable would read as "at home" and the firmware would
invent a zero wherever the arm happened to be.

## Where to start

- Building one: [docs/bom.md](docs/bom.md), then [docs/wiring.md](docs/wiring.md)
- Understanding the design: [docs/design.md](docs/design.md)
- Bringing one up: [docs/commissioning.md](docs/commissioning.md)
- Using one: [docs/operation.md](docs/operation.md)
- The CAD and its measured numbers: [cad/README.md](cad/README.md)

## Status

The mechanical design is modelled in CAD, including hard stops, switch mounts, a
chain guard and the encoder mount. It has no torque limiter, and the worm probably
does not hold the arm without power. First-pass stall loads are in
[docs/loads.md](docs/loads.md), and they show the hard stop bolts need the driver
current set down.

The firmware is written and its planning, step-rate, debounce, load, homing,
overrun, encoder and logging logic are covered by host unit tests, but **it has
never been run on hardware**. The pin assignments in
`firmware/include/config.h` are a proposal, not a built wiring loom, and the
direction sense has not been verified against a real motor. Work through
[docs/commissioning.md](docs/commissioning.md) before the arm is worn.

## License

MIT, see [LICENSE](LICENSE).
