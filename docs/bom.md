# Bill of materials

One right arm. Masses are from the Fusion model; see
[cad/README.md](../cad/README.md) for the per-body breakdown.

## Drivetrain

| Qty | Item | Mass | Notes |
|---|---|---|---|
| 1 | NEMA 23 stepper, 23HS30, 76 mm body, 1.9 N·m | — | Lumped with the gearbox at 2.2 kg in the model |
| 1 | NMRV030 worm gearbox, 15:1, 14 mm keyed output | — | Self-locking at this ratio. Do not substitute a lower ratio without rereading [safety.md](safety.md) |
| 1 | Keyed shaft hub, 14 mm bore, steel | 0.115 kg | Drives the forearm module |
| 1 | Shaft key, 5 × 5 | 0.003 kg | |
| 1 | M5 set screw | — | Retains the hub |

The ratio is a safety property, not a performance one. A 15:1 worm holds the
elbow with the motor off; a 10:1 may not, and the machine then needs holding
current and a plan for what happens when it is lost.

## Control

| Qty | Item | Mass |
|---|---|---|
| 1 | Arduino Uno | 0.008 kg |
| 1 | TB6600 stepper driver | 0.197 kg |
| 1 | E-stop, latching mushroom, **normally closed** | 0.005 kg |
| 2 | Momentary push button (UP green, DOWN black) | 0.001 kg each |
| 1 | Green status LED and series resistor | — |
| 1 | Power rocker switch | 0.002 kg |
| 1 | DC barrel jack, 24 V | 0.001 kg |
| 2 | PG7 cable gland | 0.002 kg each |
| 1 | Relay or contactor for the e-stop supply break | — | Rated for the driver's current |
| 1 | ACS712-20A current sensor module | 0.005 kg | In the 24 V feed to the driver, for load monitoring |
| 1 | Lever microswitch, **normally open** | 0.002 kg | Home switch at the extension end of travel. Polarity is deliberate: see [wiring.md](wiring.md) |

## Power

| Qty | Item |
|---|---|
| 1 | 24 V supply, 4 A or better. A current-limited bench supply for commissioning |
| 1 | 24 V to 5 V buck converter |
| 1 | Inline fuse, sized to the driver current setting |
| 1 | Electrolytic capacitor across the driver supply |

## Printed parts

PETG, FDM, 4 walls at 40% infill.

| Qty | Part | Mass |
|---|---|---|
| 1 | Control box body | 0.128 kg |
| 1 | Control box lid | 0.052 kg |
| 1 | Hand pendant housing | 0.136 kg |
| 1 | Upper arm cuff | 0.064 kg |
| 1 | Forearm cuff | 0.028 kg |
| 1 | Sling strap anchor | 0.014 kg |

PETG rather than PLA throughout: these parts sit against a body, in a car, in
sunlight, and PLA creeps under sustained load at temperatures the arm will
see.

## Structure and wearables

| Qty | Item | Mass |
|---|---|---|
| 1 | Upper strut, 1 × 1/8 in 6061 aluminium flat bar | 0.050 kg |
| 1 | Waist belt, 38 mm nylon webbing, side-release buckle | 0.094 kg |
| 1 | Sling strap, 25 mm webbing | 0.044 kg |
| 2 | Velcro cuff strap, 25 mm | 0.001 kg each |
| 1 | Upper cuff pad, 6 mm EVA foam | 0.025 kg |
| 1 | Forearm cuff pad, 5 mm EVA foam | 0.012 kg |
| 1 | 6-core pendant cable | 0.045 kg |
| — | M3–M6 fasteners, nylocs, countersunk screws | — |

## Totals

| | Mass |
|---|---|
| Upper arm module | 2.322 kg |
| Harness and electronics | 0.749 kg |
| Forearm module | 0.214 kg |
| **Device** | **3.285 kg** |

Most of it is the gearmotor, and most of that hangs at the elbow end of the
upper arm module — carried by the sling, not by the wearer's shoulder joint.
That is what the 25 mm sling strap and its printed anchor are reacting.

## Deliberately not here

- **A battery.** Commission on a bench supply with a current limit. Add a
  battery once the arm has behaved for several hours.
- **A limit switch at the flexion end.** There is now a home switch at the
  extension end, which is what the travel counter is referenced to. The
  flexion limit is still counted steps from that reference, so it is only as
  good as the counter between one homing and the next. A second switch there
  would make both ends physical. See [design.md](design.md).
- **Shoulder actuation.** Out of scope. The sling carries the shoulder.
