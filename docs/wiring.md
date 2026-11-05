# Wiring

Arduino Uno, 5 V logic. The pin assignments below are the firmware's, from
`firmware/include/config.h`; change the two together.

## Pin map

### TB6600 stepper driver

The TB6600's inputs are opto-isolated pairs. These pins drive the negative
sides with the positive sides tied to 5 V, so a pin pulled **low** activates
that input.

| Signal | Uno pin | TB6600 | Notes |
|---|---|---|---|
| Step | 3 | PUL− | 4 µs pulse, up to about 1.4 kHz at full speed |
| Direction | 4 | DIR− | Settled before the next pulse by the firmware |
| Enable | 5 | ENA− | Low energises the motor |
| 5 V | 5V | PUL+, DIR+, ENA+ | Common positive |

Set the DIP switches for **1/8 microstepping** and a current appropriate to
the motor. `cfg::kMicrosteps` must match the switches — nothing checks this,
and a mismatch scales every angle in the firmware by the same wrong factor.

### Pendant

Six-core cable to the hand unit, through a PG7 gland.

| Signal | Uno pin | Wiring |
|---|---|---|
| UP button | 6 | Momentary to GND, internal pull-up |
| DOWN button | 7 | Momentary to GND, internal pull-up |
| E-stop | 2 | **Normally closed** to GND, internal pull-up |
| GND | GND | Common return |

Two cores spare. Use one as a second ground rather than leaving it floating.

The e-stop must be the normally-closed contact of the latching mushroom. A
normally-open contact would read as "safe" when the cable is cut, which is
exactly backwards.

### Home switch

A lever microswitch at the extension end of travel, which is what gives the
firmware a real zero instead of an assumed one.

| Signal | Uno pin | Wiring |
|---|---|---|
| Home switch | 9 | **Normally open**, closing to GND, internal pull-up |

Note that this is the opposite convention to the e-stop, and for the same
reason: pick the polarity by how it fails.

- **Normally open**, as specified. A cut cable or an unplugged switch reads
  open forever. The arm never homes, so it stays on its restricted travel and
  runs slow. The failure is visible in the status output and costs range, not
  safety.
- **Normally closed**, which would be wrong here. The same cut cable reads as
  "at home", and the firmware would adopt a zero wherever the arm happened to
  be, then open up the full travel on the strength of it. A broken wire would
  silently move every limit.

The e-stop is normally closed because for a stop command, the safe assumption
on a broken wire is "stop". The home switch is normally open because for a
position reference, the safe assumption on a broken wire is "I do not know
where I am".

**Siting it matters.** The switch can only be acted on once it closes, so the
arm travels a little further while stopping, and that overshoot has to fit
between the switch and the hard stop. The firmware seeks at 5 °/s, which is
0.28° of stopping distance at 45 °/s², so a degree of clearance is ample.
Mount the lever so it closes about 1–2° before the hard stop and stays closed
all the way to it, rather than closing exactly at the stop. A switch that
closes only at the very end can be driven past before the firmware reacts.

### Panel

| Signal | Uno pin | Notes |
|---|---|---|
| Status LED | 8 | Green LED on the control box face, with its series resistor |
| Onboard LED | 13 | Mirrors the status LED, useful on the bench |
| Current sense | A0 | ACS712-20A output, 100 mV/A about a mid-rail zero |

Pins 10 to 12 and A1 to A5 are unused. A flexion-end limit switch would be
the obvious next claim on one of them.

### Current sensor

An ACS712-20A goes **in the 24 V feed to the TB6600**, downstream of the
e-stop contact, so it sees the driver's draw and nothing else. Its 5 V and
ground come from the Arduino, and its output goes to A0.

Put it on the driver feed, not on the supply input: the buck converter and
the Arduino would otherwise appear in the reading as a constant offset that
drifts with whatever the logic is doing.

The firmware measures the sensor's zero at every power-on, with the driver
de-energised. Nothing may be moving during the first second after power-up or
that reading becomes the zero point for the whole session.

## E-stop, in hardware as well as firmware

The firmware reads the e-stop and stops. That is not enough on its own: it
depends on the firmware working. The same loop must also break the **24 V
supply to the TB6600**, through a relay or a contactor rated for it.

```
24 V ──[fuse]──┬──── E-STOP NC contact ────── TB6600 VCC
               │
               └──── 24 V → 5 V buck ──────── Arduino VIN
```

Wired this way the Arduino stays alive with the loop open, so it can see the
fault, light the LED and refuse to resume — while the motor is dead
regardless of what any code does.

Do not power the Arduino from USB and the buck at the same time while the
motor supply is live.

## Power

| Item | Value |
|---|---|
| Supply | 24 V, at least 4 A for a NEMA 23 at full current |
| Fuse | Inline, sized to the driver's current setting |
| Bulk capacitance | Electrolytic across the TB6600 supply terminals, as close as the layout allows |
| Logic | 24 V → 5 V buck to the Arduino VIN |

A stepper driver chops current continuously, and the 24 V rail sags hard on
direction reversals. Commission on a current-limited bench supply before
anything else.

## Cabling

The motor cable crosses the elbow and flexes on every cycle. It is the cable
that will eventually fail. Give it a service loop, strain-relieve both ends,
and use a locking connector so replacing it is a five-minute job rather than
a rebuild.

Keep the motor cable away from the pendant cable. A TB6600 switching amps at
the far end of a long run will couple into button lines, and a false button
read is motion nobody asked for.
