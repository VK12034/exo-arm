# exo-arm

![BudgetExoArm](cad/BudgetExoArm.png)

A single-degree-of-freedom powered elbow worn on the right arm. A NEMA 23
stepper drives the elbow through a 15:1 worm; a sling and waist belt carry
the shoulder; the wearer commands it with a hand pendant.

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
| Output | ≈ 20 N·m at the elbow after worm losses, against ≈ 2.8 N·m of load |
| Resolution | 24 000 microsteps per output revolution, 66.7 per degree |
| Travel | 0° (full extension) to 135° of flexion |
| Control | Arduino Uno and a TB6600 driver, belt-mounted |
| Interface | UP / DOWN buttons and a latching NC e-stop on a hand pendant |
| Device mass | 3.285 kg |

The worm is the fact that shapes everything else. At 15:1 it is self-locking:
the elbow holds its angle with the motor de-energised, so there is no holding
current, no gravity compensation to compute, and nothing to tune. It also
means the wearer cannot backdrive the arm, which is why the e-stop cuts
driver power rather than asking the firmware to be well behaved.

## Layout

| Path | What is in it |
|---|---|
| `firmware/` | Arduino Uno firmware: motion planning, step generation, interlocks |
| `cad/` | The Fusion design, STEP and STL exports, and measured mass properties |
| `docs/` | Design, safety, wiring, BOM, commissioning, operation |
| `tools/fusion/` | Read-only Fusion script that regenerates everything in `cad/` |

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
sounds — a stepper stopped dead at speed loses steps, and with no encoder the
step counter is the only idea the firmware has of where the arm is.

Three things are enforced below the planner: the step generator refuses to
pulse past the travel limits whatever it is told, the e-stop drops the driver
enable before anything else runs, and both buttons at once reads as stop
rather than as a guess.

## Safety model

The e-stop loop is normally closed to ground. A pressed mushroom, an
unplugged pendant and a cut cable all read identically: open, meaning stop.
It breaks the driver supply in hardware as well as being read by the
firmware, so the motor dies whether or not the Arduino is behaving.

At power-on the firmware starts in a fault state and will not leave it until
the e-stop reads closed **and** both buttons are released, so a stuck button
cannot drive the arm the moment power arrives.

## Where to start

- Building one: [docs/bom.md](docs/bom.md), then [docs/wiring.md](docs/wiring.md)
- Understanding the design: [docs/design.md](docs/design.md)
- Bringing one up: [docs/commissioning.md](docs/commissioning.md)
- Using one: [docs/operation.md](docs/operation.md)
- The CAD and its measured numbers: [cad/README.md](cad/README.md)

## Status

The mechanical design is complete in CAD. The firmware is written and its
planning, step-rate and debounce logic are unit tested, but **it has never
been run on hardware**. The pin assignments in `firmware/include/config.h`
are a proposal, not a built wiring loom, and the direction sense has not been
verified against a real motor. Work through
[docs/commissioning.md](docs/commissioning.md) before the arm is worn.

## License

MIT, see [LICENSE](LICENSE).
