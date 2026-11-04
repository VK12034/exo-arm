# exo-arm

A two-degree-of-freedom powered exoskeleton for one arm: Teensy firmware,
host-side control and telemetry tools, and the documentation needed to build
and commission it.

The arm supports shoulder and elbow flexion in the sagittal plane. A
closed-form gravity model carries the weight, so the position loop only has
to correct the residual and the gains can stay low enough for the arm to feel
transparent to wear.

> **This is a workshop build, not a product.** It is not a medical device, it
> is not certified, and it is a powered machine attached to a human arm. Read
> [docs/safety.md](docs/safety.md) before anyone puts it on.

## Layout

| Path | What is in it |
|---|---|
| `firmware/` | Teensy 4.1 firmware: control loop, safety state machine, serial link |
| `host/` | Python package: protocol, live plotting, CLI, CSV logging |
| `docs/` | Design, safety, wiring, BOM, commissioning |
| `cad/` | Mechanical source, STEP exports and drawings |

## Firmware

```bash
cd firmware
pio run -e teensy41            # build
pio run -e teensy41 -t upload  # flash
pio test -e native             # unit tests for the maths, on the host
```

The control loop runs at 1 kHz from a hardware timer, independent of whatever
the serial link is doing. Each step applies gravity feed-forward, a position
PID with derivative on measurement, soft-limit shaping, and velocity and
current ceilings before the duty reaches the driver.

## Host tools

```bash
cd host
pip install -e ".[plot,dev]"

exoarm --port COM5 status
exoarm --port COM5 monitor              # live plot
exoarm --port COM5 mode assist
exoarm --port COM5 record --seconds 30 --out run.csv
pytest
```

## Safety model

Three independent interlocks must all pass before any joint drives: a
normally-closed e-stop loop, a deadman grip switch, and no latched fault.
Faults latch until cleared explicitly, and the firmware refuses to clear one
while its cause is still present.

The Teensy owns every safety decision. The host can request modes, targets
and gains, but it cannot override an interlock, and the arm idles itself if
the host goes quiet for 500 ms. A laptop crashing must not be able to leave a
motor driving.

## Where to start

- Building one: [docs/bom.md](docs/bom.md), then
  [docs/wiring.md](docs/wiring.md)
- Understanding the design: [docs/design.md](docs/design.md)
- Bringing one up: [docs/tuning.md](docs/tuning.md)
- Using one: [docs/operation.md](docs/operation.md)

## Status

Early. The firmware and host tools are written and the hardware-independent
maths is unit tested, but nothing here has been run against real hardware
yet. Treat every constant in `config.h` as a starting estimate to be measured
and replaced during commissioning.

## License

MIT, see [LICENSE](LICENSE).
