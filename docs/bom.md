# Bill of materials

One right arm. Masses are from the Fusion model; see
[cad/README.md](../cad/README.md) for the per-body breakdown.

## Drivetrain

| Qty | Item | Mass | Notes |
|---|---|---|---|
| 1 | NEMA 23 stepper, 23HS30, 76 mm body, 1.9 N·m | — | Lumped with the gearbox at 2.19 kg in the model |
| 1 | NMRV030 worm gearbox, 15:1, 14 mm keyed output | — | **Probably does not hold a loaded arm without current**: see [loads.md](loads.md) |
| 1 | #35 sprocket, 15T, 14 mm keyed bore | 0.084 kg | On the gearbox output |
| 1 | #35 sprocket, 30T, plate type, drilled | 0.256 kg | On the elbow |
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
| 1 | Encoder magnet cup | 0.004 kg |

PETG rather than PLA throughout: these parts sit against a body, in a car, in
sunlight, and PLA creeps under sustained load at temperatures the arm will
see.

## Structure and wearables

| Qty | Item | Mass |
|---|---|---|
| 1 | Upper strut, 1.5 × 3/16 in 6061 flat bar | 0.143 kg |
| 1 | Forearm strut, 1 × 3/16 in 6061 flat bar | 0.086 kg |
| 2 | Hard stop: 7 mm steel spacer, 10 mm OD rubber sleeve, M5 bolt | 0.003 kg each |
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
| 2 | Longer M5 sprocket bolts and 2 extra nuts, to clamp the magnet cup | — |

## Totals

| | Mass |
|---|---|
| Upper arm module | 2.907 kg |
| Harness and electronics | 0.765 kg |
| Forearm module | 0.430 kg |
| Chain guard | 0.044 kg |
| Switch mounts | 0.013 kg |
| **Device** | **4.160 kg** |

Most of it is the gearmotor, mounted mid upper arm and carried by the sling
rather than the wearer's shoulder joint. That is what the 25 mm sling strap and
its printed anchor are reacting.

## Deliberately not here

- **A battery.** Commission on a bench supply with a current limit. Add a
  battery once the arm has behaved for several hours.
- **A torque limiter.** Not designed yet, and needed: see [loads.md](loads.md).
- **A brake, or a 30:1 worm with a 1:1 chain.** Needed if the holding test fails, which
  the catalogue figures suggest it will.
- **Shoulder actuation.** Out of scope. The sling carries the shoulder.
