# Torque limiter

A friction clutch at the 30T sprocket, so the motor drives the forearm through a joint
that slips at a set torque. It is modelled but has not been built or tested. Figures
are reproducible with `python tools/clutch_sizing.py`.

**This is a design proposal, not a proven safety device.** The friction coefficient is
assumed, slip torque will drift with wear, and 41 N has not been assessed as safe for
an arm. No medical or engineering reviewer has seen it.

## What it does

| At the elbow | Without the clutch, driver at 2.0 A | With it set to 8 N·m |
|---|---|---|
| Most torque | 8.9 to 15.1 N·m | About 8 N·m |
| Force at the cuff | 46 to 78 N | About 41 N |
| Force on a hard stop | 367 to 621 N | About 330 N |
| Margin over the 2.9 N·m static load | 3.1 to 5.2 times | 2.8 times |

It works in both directions and needs no power or firmware. Force rises closer to the
elbow, because the limit is a torque: about 80 N at 100 mm from the pivot. If the clutch
seizes or is over-tightened, the motor and driver setting are the only limit left.

The slip torque also caps what the arm can carry. At 8 N·m, about 5 N·m is left after the
static load, or roughly 2.7 kg at the cuff, before it slips. That is less in practice,
because the setting drifts downward with wear.

## How it is built

From the forearm strut outward, along the elbow axis:

| y (mm) | Part | Notes |
|---|---|---|
| 72.6 to 74.1 | Shim, steel, 46 mm OD, 1.5 mm | Lifts the plate clear of the hard stops and switches |
| 74.1 to 76.6 | Inner plate, steel, 78 mm OD, 2.5 mm | Fixed to the forearm strut |
| 76.6 to 77.6 | Inner facing, 1 mm, 54 to 77 mm | Bonded to the inner plate |
| 77.6 to 82.9 | 30T sprocket, bored to 52 mm | Same position, so the chain line does not move |
| 76.6 to 83.4 | Hub, 6061, 51 mm OD | Inside the sprocket's bore, and keeps it centred |
| 82.9 to 83.9 | Outer facing, 1 mm, 54 to 77 mm | Bonded to the outer plate |
| 83.9 to 86.9 | Outer plate, steel, 78 mm OD, 3 mm | Rotates with the forearm, free to move along the bolts |
| 86.9 to 89.7 | 4 disc springs on each bolt, in series | DIN 2093, 10 × 5.2 × 0.4 |
| 89.7 to 93.7 | M5 nylock on each bolt | Sets the slip torque |

Four M5 bolts at 20 mm from the axis hold it together. Two are the existing flat-head
bolts through the strut. The other two sit either side of the strut, countersunk through
the shim and inner plate, which the model does not show. Cut each bolt so it ends no
more than 1.5 mm past its nut.

The sprocket has no bolts through it, so it can slip any distance. The hub stops 0.5 mm
short of the outer plate, so all the clamping force passes through the facings.

The encoder magnet cup now screws to the outer plate with 4 M3 screws, and the nuts and
springs pass through holes in it. **This keeps the magnet on the forearm side of the
clutch.** Had it stayed on the sprocket, it would have turned with the motor and never
seen a slip.

The pivot needs one more part, which is not in the model yet. Nothing else locates the
forearm along the pivot bolt, so it could slide about 9 mm and rub the plates on the
switches. Add a spacer, 13 mm OD by 8.2 mm ID, from y = 77.6 to the nylock at y = 87, cut
so the forearm has about 0.4 mm of end play. It passes through the hub and outer plate
without touching them.

Two earlier parts had to change for the plates to fit:

- The 5 mm sprocket spacers are gone.
- The hard stop bolts are now M5 × 10 into tapped spacers, ending below the plate. A
  longer bolt, or a nut on top of the spacer, would hit the inner plate.

## Sizing

Slip torque is 2 × friction × clamp force × 33.1 mm, the facing's mean radius.

| Friction assumed | Clamp force | Per bolt | Spring stack travel | Nut turns |
|---|---|---|---|---|
| 0.15 | 806 N | 201 N | 0.85 mm | 1.07 |
| 0.25 | 484 N | 121 N | 0.44 mm | 0.55 |
| 0.40 | 302 N | 76 N | 0.25 mm | 0.32 |

The 0.25 is for an organic facing at room temperature and comes from one patent, so it
is only a starting point. That is why the setting is measured rather than calculated.

**Facing material:** 1 mm non-asbestos clutch facing or gasket sheet, bonded with contact
cement. Try a sample before committing, since some gasket sheets are too soft.

**Keep oil off the facings.** Oil could cut the friction enough for the forearm to slide
under its own weight. Use a dry-film chain lubricant rather than oil.

## Setting it

Do this unworn, with the driver on and the arm stopped, after step 10 of
[commissioning.md](commissioning.md). The motor holding still is what the clutch slips
against. Pull with a spring scale at the forearm cuff, at right angles to the forearm.

1. With the magnet cup off, turn all four nuts down until the springs just start to
   load, then turn each a further half turn.
2. Fit the cup. Pull until the forearm moves, and note the reading.
3. Adjust the nuts evenly in eighth turns, taking the cup off each time, until it slips at
   **35 to 45 N**. That is about 6.8 to 8.7 N·m.
4. Confirm that the firmware reports `FAULT: encoder disagrees` when it slips.
5. Release the buttons to clear the fault, extend the arm onto the home switch to re-home,
   and repeat three times. The
   readings should agree within about 5 N. Sticking or juddering means the facings or
   sprocket face need attention.

Never go above 50 N at the cuff, and stop if the arm cannot carry its load at 35 N.
Raising the setting removes the protection this exists to give.

## What to check every session

Pull at the cuff with the scale, unworn, as in the setting steps.

| Result | Meaning |
|---|---|
| Holds 30 N, slips by 50 N | In range |
| Slips below 30 N | Worn or contaminated. Do not wear it. Adjust or replace the facings |
| Still holding at 50 N | Over-tightened or seized. Do not wear it |

Wear lowers the setting quickly, because the springs have little travel. From 8 N·m, the
calculation gives 6.4 N·m after 0.10 mm of wear and 2.9 N·m after 0.30 mm, at which point
the forearm can sag. Replace the facings if the nuts have been taken up by a total of
about half a turn, since the hub touches the outer plate after 0.5 mm.

## What happens when it slips

The forearm stops following the motor, so the encoder sees the count and angle disagree.
Past 3° for 400 ms, the firmware faults, stops the motor and drops to restricted travel
until the arm is moved back onto the home switch. At 20°/s that lets about 11° of slip
through before the motor stops, but the clutch still limits the force during that time.

The fault leaves motor current on, so the forearm stays pressed at the slip torque until
the obstruction is removed or the e-stop is pressed. Pressing the e-stop removes that
torque only if the worm then gives way, so the straps remain the way out.

## What has not been checked

- The real friction coefficient, how much it differs between sticking and sliding, and
  how it changes with heat, humidity and wear.
- Whether two steel plates this thin stay flat enough to load the facings evenly. Only
  a rough estimate was done, suggesting tens of micrometres of bending.
- Boring a plate sprocket to 52 mm. It needs a lathe or a good hole saw, and must stay
  concentric with the teeth.
- The plates clear the overrun switch by 0.8 mm and the chain by about 2 mm, as modelled.
  Check both on the real parts.
- Whether this slip torque suits the wearer. It is an engineering estimate, not an
  injury threshold.
