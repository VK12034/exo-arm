# Bill of materials

Quantities are for one arm. Prices are rough and from late 2026; treat them
as a sanity check, not a quote.

## Actuation

| Qty | Item | Notes |
|---|---|---|
| 1 | Brushed gearmotor, ~100:1, 24 V, 2048 CPR encoder | Shoulder. Needs roughly 26 Nm holding torque at the output |
| 1 | Brushed gearmotor, ~70:1, 24 V, 2048 CPR encoder | Elbow. Roughly 12 Nm |
| 2 | Pololu G2 High-Power Motor Driver 24v13 | Current sense and a fault output matter more than the headline rating |
| 2 | Shaft coupling, backlash-free | Backlash here shows up directly as the arm feeling vague |

Size the gear ratio for backdrivability, not just torque. A ratio high enough
to hold the arm effortlessly is also high enough to make it unpleasant to
move when unpowered.

## Control

| Qty | Item |
|---|---|
| 1 | Teensy 4.1 |
| 1 | Protoboard or a small custom PCB for the breakouts |
| 2 | Load cell, 0-20 kg, with an instrumentation amp |
| 1 | E-stop, normally closed, latching |
| 1 | Momentary grip switch (deadman) |

Prefer an analog instrumentation amp over an HX711 for the load cells: the
HX711 80 Hz ceiling is slow enough to be felt in an admittance loop.

## Power

| Qty | Item |
|---|---|
| 1 | 24 V supply, 10 A. A current-limited bench supply for commissioning |
| 1 | 24 V to 5 V buck, 2 A |
| 1 | Inline fuse, 15 A |
| 2 | 1000 uF electrolytic, 35 V |

## Structure

| Qty | Item | Notes |
|---|---|---|
| 2 | Aluminium link, 6061, machined or waterjet | See the cad/ folder |
| 1 | Shoulder mount and harness interface | Takes the full reaction torque, so overbuild it |
| 3 | Cuff, padded, quick release | Upper arm, forearm, torso |
| - | Bearings, shoulder bolts, fasteners | Sized per the CAD |

## Deliberately not here

- A battery. Commission on a bench supply with a current limit. Add a battery
  once the arm has behaved for a few hours, not before.
- Wrist or pronation actuation. Out of scope; see design.md.
