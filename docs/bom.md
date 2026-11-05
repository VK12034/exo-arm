# Bill of materials

One right arm. Masses are from the Fusion model; see
[cad/README.md](../cad/README.md) for the per-body breakdown.

## Drivetrain

| Qty | Item | Mass | Notes |
|---|---|---|---|
| 1 | NEMA 23 stepper, 23HS16-2804, 41 mm body, 0.55 N·m, 2.8 A, 1.4 mH | 0.55 kg gross, per the listing | Do not swap for a stronger motor, because this one sets the force ceiling. Do not use the high-inductance 0.88 A version. Check the shaft size and the torque at 200 rpm on 24 V before ordering. The rear shaft of a dual-shaft motor spins exposed, so cover it |
| 1 | NMRV030 worm gearbox, 30:1, 14 mm keyed output | 1.10 kg, estimated | Single-start. **Should hold a loaded arm without current, untested**: see [loads.md](loads.md). A NEMA 23 motor needs an adapter to its input, so check that one exists for the 30:1 input bore |
| 1 | #35 sprocket, 15T, 14 mm keyed bore | 0.084 kg | On the gearbox output |
| 1 | #35 sprocket, 30T, plate type, bored to 52 mm | 0.174 kg after boring | On the elbow, free to slip inside the clutch. The bore must be concentric with the teeth |
| 1 | #35 roller chain, 56 links | 0.270 kg | No tensioner is modelled |
| 1 | Shaft key, 5 × 5 | 0.003 kg | |
| 1 | M8 shoulder bolt, nylock and 2 bronze 8 × 12 bushings | 0.025 kg | Elbow pivot |
| 1 | 1/4 in shock cord, 3 loops, with anchor and spring pins | 0.046 kg | Assist |

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
| 1 | AS5600 magnetic encoder board | 0.001 kg | Over the elbow axis, on the chain guard. Check its supply voltage |
| 1 | Diametric magnet, 6 × 2.5 mm | 0.001 kg | Must be diametrically magnetised |
| 1 | ACS712-20A current sensor module | 0.005 kg | In the 24 V feed to the driver, for load monitoring |
| 2 | Lever microswitch, **normally open** | 0.002 kg each | KW12-style. Home switch at the extension stop, overrun switch past the flexion limit. Polarity is deliberate: see [wiring.md](wiring.md) |

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
| 1 | Upper arm cuff | 0.046 kg |
| 1 | Forearm cuff | 0.039 kg |
| 1 | Gearbox plate, 10 mm, with heat-set inserts | 0.047 kg |
| 1 | Sling strap anchor | 0.014 kg |
| 1 | Chain guard, 2 mm walls | 0.043 kg |
| 1 | Encoder magnet cup | 0.003 kg |

PETG rather than PLA throughout: these parts sit against a body, in a car, in
sunlight, and PLA creeps under sustained load at temperatures the arm will
see.

## Structure and wearables

| Qty | Item | Mass |
|---|---|---|
| 1 | Upper strut, 1.5 × 3/16 in 6061 flat bar | 0.143 kg |
| 1 | Forearm strut, 1 × 3/16 in 6061 flat bar | 0.086 kg |
| 2 | Hard stop: 7 mm steel spacer tapped M5, 10 mm OD rubber sleeve, M5 × 10 bolt | 0.003 kg each |
| 2 | Switch bracket, 3 mm 6061, with 2 × 5 mm OD spacers | 0.005 kg each |
| 3 | Aluminium standoff, 10 mm OD, for the gearbox plate | 0.003 kg each |
| 1 | Waist belt, 38 mm nylon webbing, side-release buckle | 0.094 kg |
| 1 | Sling strap, 25 mm webbing | 0.044 kg |
| 2 | Velcro cuff strap, 25 mm | 0.002 kg each |
| 1 | Upper cuff pad, 5 mm EVA foam | 0.021 kg |
| 1 | Forearm cuff pad, 5 mm EVA foam | 0.012 kg |
| 1 | 6-core pendant cable | 0.045 kg |
| 1 | 4-core motor cable, 18 AWG | 0.016 kg |
| — | M3–M6 fasteners, nylocs, countersunk screws | — |
| 2 | M3 screws for the guard pillars, through the upper strut | — |
| 4 | M3 screws for the magnet cup, into the clutch's outer plate | — |

## Torque limiter

See [torque-limiter.md](torque-limiter.md).

| Qty | Item | Mass | Notes |
|---|---|---|---|
| 1 | Inner plate, steel, 78 mm OD × 14 mm ID × 2.5 mm, 4 holes | 0.089 kg | Laser cut, flat |
| 1 | Outer plate, steel, 78 mm OD × 18 mm ID × 3 mm, 4 holes, 4 M3 tapped | 0.104 kg | Laser cut, flat |
| 1 | Shim, steel, 46 mm OD × 14 mm ID × 1.5 mm, 4 holes | 0.017 kg | |
| 1 | Hub, 6061, 51 mm OD × 16 mm ID × 6.8 mm, 4 holes | 0.032 kg | Its height leaves a 0.5 mm gap to the outer plate, so that the facings take the clamping force |
| 2 | Friction facing, 1 mm, 77 mm OD × 54 mm ID | 0.004 kg each | Bonded to each plate. Keep oil off them |
| 16 | Disc spring, DIN 2093, 10 × 5.2 × 0.4 | — | 4 per bolt, in series |
| 2 | M5 flat-head bolt, cut to about 27 mm overall | — | Through the forearm strut, replacing the sprocket bolts |
| 2 | M5 flat-head bolt, cut to about 22 mm overall | — | Through the shim and inner plate, either side of the strut |
| 4 | M5 nylock nut | 0.001 kg each | Sets the slip torque |
| 1 | Spring scale, 0 to 100 N | — | Not part of the arm. Needed to set and check the clutch |

## Totals

| | Mass |
|---|---|
| Upper arm module | 2.365 kg |
| Harness and electronics | 0.764 kg |
| Forearm module | 0.600 kg |
| Chain guard | 0.044 kg |
| Switch mounts | 0.013 kg |
| **Device** | **3.787 kg** |

Most of it is the gearmotor, mounted mid upper arm and carried by the sling
rather than the wearer's shoulder joint. That is what the 25 mm sling strap and
its printed anchor are reacting.

## Deliberately not here

- **A battery.** Commission on a bench supply with a current limit. Add a
  battery once the arm has behaved for several hours.
- **A motor brake.** Needed if the holding test fails, which the catalogue figures
  suggest it should not.
- **Shoulder actuation.** Out of scope. The sling carries the shoulder.
