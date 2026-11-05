# Loads and the worm

First-pass figures, reproducible with `python tools/stall_loads.py`. They size the
problem, and they are not a structural sign-off. Limits are typical published
values rather than figures for the parts you will buy.

## Does the worm hold the arm?

**By the manufacturer's own numbers, probably not reliably.** From Motovario's
technical catalogue, sections 2.4 and 2.5, for an NMRV 030 at 15:1:

| Figure | Value | Catalogue's class |
|---|---|---|
| Lead angle | 9°40′ | |
| Static efficiency | 0.55 | Above 0.5, so not statically irreversible. 0.5 to 0.55 is "low static reversibility", and above 0.55 is "static reversibility" |
| Dynamic efficiency at 1400 rpm | 0.78 | Above 0.6 is "dynamic reversibility" |

The catalogue requires a static efficiency below 0.5 for a load on the output to
fail to turn the worm, and warns that vibration and shocks can affect this even
then. The gearmotor modelled here may come from another maker, whose figures can
differ, so [the holding test](commissioning.md) on the real gearbox decides.

Plan on the forearm being able to move whenever motor current is lost.

### The option worth considering

The same table gives the 030 at 30:1 a static efficiency of 0.39 and a dynamic
efficiency of 0.66, which the catalogue classes as statically irreversible. A
30:1 worm with a 1:1 chain would keep today's overall ratio, so
`kStepsPerDegree` would not change. It has not been designed: sprocket fit, that
gearbox's torque rating, and the chain tension all need checking. A brake on the
motor is the other route.

## Stall loads

At full motor holding torque of 1.9 N·m, using the catalogue efficiencies as the
range. Static load is 2.9 N·m, assuming a 70 kg wearer with the forearm horizontal.

| Worm efficiency | Gearbox output | Elbow | Times static | At the cuff | Chain | On a hard stop |
|---|---|---|---|---|---|---|
| 0.55 | 15.7 N·m | 30.4 N·m | 10.4 | 158 N | 684 N | 1252 N |
| 0.78 | 22.2 N·m | 43.1 N·m | 14.8 | 223 N | 970 N | 1776 N |

About 160 to 220 N at the cuff, roughly the weight of 16 to 23 kg, is what could
reach an arm with no torque limiter.

| Item | Limit | Result |
|---|---|---|
| Gearbox output | 25 N·m listed maximum | 22 N·m at most. Little margin |
| #35 chain | About 2135 N allowable, 7830 N minimum tensile | Comfortable |
| M5 hard stop bolt in bending, if the clamp does not help | 34 N·m at the elbow | **Exceeded at the top of the range** |
| M5 hard stop bolt in shear | 132 N·m at the elbow | Comfortable |
| M5 bolt bearing in the 4.8 mm strut | 225 N·m at the elbow | Comfortable |

Not checked: contact stress where the strut edge meets the spacer, the strut's
own bending, the gearbox plate, and the shoulder mount.

## What this means

**Set the driver current down.** With a factor of 2 on the weakest hard stop check,
elbow torque should stay under about 17 N·m. At the higher efficiency that is about
0.75 N·m at the motor, or roughly 40% of its rated current if torque follows current
in proportion. That still leaves about six times the static load, before the drop in
torque with speed. Use it as a starting point for the current step in
[commissioning.md](commissioning.md), and confirm it moves a loaded arm without
skipping.

**A torque limiter is still needed.** Lowering current lowers the ceiling, but it is
a setting rather than a guarantee. A friction clutch between the 30T sprocket and the
forearm strut, set to slip near that same torque, would hold when the firmware or the
DIP switches are wrong. It is not designed yet. If it slips, the encoder sees it.

## Sources

- [Motovario technical catalogue, VSF/IEC/ATEX rev1 2017](https://my.motovario.com/uploads/pdf_static/TECHNICAL%20CATALOGUE_VSF_IEC_ATEX_EN_rev1_2017.pdf), read from a PDF text extraction, so check the table against the page
- [RS Components listing for a Motovario 15:1 worm gearbox](https://za.rs-online.com/web/p/gearboxes/2166479), 25 N·m maximum
- [Tsubaki RS35 roller chain](https://chains.ustsubaki.com/viewitems/rs25-through-rs240-chains/rs35-roller-chains), 1760 lb minimum
- [partmatch #35 chain](https://partmatch.io/chain/35-1), 480 lb maximum allowable load
