# Loads and the worm

First-pass figures, reproducible with `python tools/stall_loads.py`. They size the
problem, and they are not a structural sign-off. Limits are typical published
values rather than figures for the parts you will buy.

## The drivetrain, and why

A 30:1 worm, the 15T to 30T chain, and a 0.55 N·m motor, for 60:1 overall. All figures
below are at full motor torque.

| | 15:1 worm, chain, 1.9 N·m motor | 30:1 worm, 1:1 chain, 1.9 N·m motor | **This design** |
|---|---|---|---|
| Catalogue static efficiency | 0.55, does not hold | 0.39, should hold | **0.39, should hold** |
| Force at the cuff | 158 to 223 N | 112 to 189 N | **65 to 109 N** |
| Elbow torque | 30 to 43 N·m | 22 to 37 N·m | **12 to 21 N·m** |
| Peak gearbox torque | 22 N·m | 38 N·m | **11 N·m** |
| Parts that change from the 15:1 design | | Sprocket, chain, guard, encoder mount | **Motor and gearbox** |

A stronger motor with its current turned down could reach the same force, but only
while the DIP switches stay right. Here, the motor is what limits it.

The cost is speed and smoothness at the motor. It turns at 200 rpm for 20°/s, twice the
earlier design, and it runs at 1/4 microstepping to keep the step rate and
`kStepsPerDegree` unchanged. That is rougher than 1/8, and vibration can affect a
worm's holding.

## Does the worm hold the arm?

**By the manufacturer's numbers it should, when stopped.** From Motovario's technical
catalogue, sections 2.4 and 2.5, for an NMRV 030 at 30:1:

| Figure | Value | Catalogue's class |
|---|---|---|
| Lead angle | 4°52′ | |
| Static efficiency | 0.39 | Below 0.5, so statically irreversible |
| Dynamic efficiency at 1400 rpm | 0.66 | Above 0.6 is "dynamic reversibility" |

The catalogue requires a static efficiency below 0.5 for a load on the output to
fail to turn the worm, and warns that vibration and shocks can affect this even
then. The dynamic figure means a moving worm is not expected to stop instantly when
drive is removed, although this one runs far below 1400 rpm. Other makers' figures can
differ, so [the holding test](commissioning.md) on the real gearbox decides. If it
fails, the fix is a motor brake.

A worm that holds has a cost of its own: the arm stays locked where it stopped, and it
cannot be pushed back by hand. The quick-release straps are the way out.

## Stall loads

Using the catalogue efficiencies as the range. Static load is 2.9 N·m, assuming a 70 kg
wearer with the forearm horizontal.

| Driver | Worm efficiency | Motor | Gearbox output | Elbow | Times static | At the cuff | Chain | On a hard stop |
|---|---|---|---|---|---|---|---|---|
| 2.0 A, start here | 0.39 | 0.39 N·m | 4.6 N·m | 8.9 N·m | 3.1 | 46 N | 201 N | 367 N |
| 2.0 A, start here | 0.66 | 0.39 N·m | 7.8 N·m | 15.1 N·m | 5.2 | 78 N | 339 N | 621 N |
| 2.8 A, rated | 0.39 | 0.55 N·m | 6.4 N·m | 12.5 N·m | 4.3 | 65 N | 281 N | 514 N |
| 2.8 A, rated | 0.66 | 0.55 N·m | 10.9 N·m | 21.1 N·m | 7.3 | 109 N | 475 N | 870 N |
| 3.5 A, wrong switch | 0.66 | 0.69 N·m | 13.6 N·m | 26.4 N·m | 9.1 | 137 N | 594 N | 1087 N |

The 3.5 A row assumes torque keeps rising with current, which is generous because the
motor saturates, and running there would overheat it. Torque here is proportional to
current and comes from holding torque, so it falls off somewhat with speed.

| Item | Limit | Result |
|---|---|---|
| Gearbox output, section 5.12 at 500 rpm input | 20 N·m rated, 40 N·m peak | 10.9 N·m at rated motor current. Comfortable |
| Radial load on the output, chain wheel factor 1.4 | 1529 N on the centre line, 994 N if 50 mm out | 665 N at rated motor current. Comfortable |
| #35 chain | About 2135 N allowable, 7830 N minimum tensile | Comfortable |
| M5 hard stop bolt in bending, if the clamp does not help | 34 N·m at the elbow | Not reached, even in the 3.5 A row |
| M5 hard stop bolt in shear | 132 N·m at the elbow | Comfortable |
| M5 bolt bearing in the 4.8 mm strut | 225 N·m at the elbow | Comfortable |

Not checked: contact stress where the strut edge meets the spacer, the strut's
own bending, the gearbox plate, the shoulder mount, and the clutch plates' flatness.

## Is the motor strong enough?

Carrying the static load at the lower efficiency takes about 0.13 N·m at the motor. At
2.0 A it has about 0.39 N·m, or 3.1 times that, before the drop with speed and before a
carried load.

At 20°/s the motor takes 1.50 ms per full step, and with 1.4 mH on 24 V the phase
current takes about 0.16 ms to rise, so little torque should be lost to speed. That is
a calculation, and no torque curve for this motor has been checked, so read one before
ordering, and let step 10 of [commissioning.md](commissioning.md) decide.

Avoid the 0.88 A, 21.7 mH version sold under a similar name. Its current would take
far longer to rise at this speed.

## What this means

**Start the driver at 2.0 A, and never set it above 2.8 A.** With a factor of 2 on the
weakest hard stop check, elbow torque should stay under about 17 N·m, and 2.0 A does.
Rated current is within the bolt's limit with a factor of about 1.6. Confirm that 2.0 A
moves a loaded arm without skipping.

**The clutch sets the real limit, if it works.** About 80 to 110 N at the cuff can still
hurt a pinned arm. [A friction clutch](torque-limiter.md) between the 30T sprocket and
the forearm, set to slip near 8 N·m, should bring that to about 41 N and about 330 N on a
hard stop, whatever the firmware or DIP switches do. The figures above are what is left
if it seizes or is over-tightened. It has not been built or tested.

## Sources

- [Motovario technical catalogue, VSF/IEC/ATEX rev1 2017](https://my.motovario.com/uploads/pdf_static/TECHNICAL%20CATALOGUE_VSF_IEC_ATEX_EN_rev1_2017.pdf), read from a PDF text extraction whose columns were scrambled, so check the tables against the page
- [StepperOnline 23HS16-2804D listing](https://www.omc-stepperonline.com/dual-shaft-nema-23-bipolar-1-8deg-0-55nm-78oz-in-2-8a-2v-57x57x41mm-4-wires.html), from a search result because the page itself refused the request
- [Tsubaki RS35 roller chain](https://chains.ustsubaki.com/viewitems/rs25-through-rs240-chains/rs35-roller-chains), 1760 lb minimum
- [partmatch #35 chain](https://partmatch.io/chain/35-1), 480 lb maximum allowable load
