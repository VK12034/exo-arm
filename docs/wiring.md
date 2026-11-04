# Wiring

Teensy 4.1. All logic is 3.3 V and **not** 5 V tolerant, so anything coming
from a 5 V part goes through a divider or a level shifter.

## Pin map

Mirrors `firmware/include/config.h`. Change them together.

### Motor drivers (Pololu G2 High-Power 24v13, one per joint)

| Signal | J0 shoulder | J1 elbow | Notes |
|---|---|---|---|
| PWM | 2 | 6 | 20 kHz, 12-bit |
| DIR | 3 | 7 | |
| SLP | 4 | 8 | Active low; held low until the joint is enabled |
| FLT | 5 | 9 | Open drain, internal pull-up, low on fault |
| CS | A0 | A1 | 20 mV/A, analog |

### Encoders (2048 CPR quadrature, on the motor shaft)

| Signal | J0 | J1 |
|---|---|---|
| A | 14 | 16 |
| B | 15 | 17 |

Both channels need interrupt-capable pins. Use shielded cable and ground the
shield at the Teensy end only; long unshielded encoder runs next to motor
leads will give you phantom counts.

### Safety and sensing

| Signal | Pin | Notes |
|---|---|---|
| E-stop loop | 20 | Normally closed to GND, internal pull-up |
| Deadman grip | 21 | Active low, internal pull-up |
| Status LED | 13 | On-board |
| Force sense J0 | A2 | Load cell amp, mid-rail at zero force |
| Force sense J1 | A3 | |

## Power

    24 V supply --+-- driver J0 -- motor J0
                  +-- driver J1 -- motor J1
                  +-- 5 V buck  -- Teensy VIN

- **Cut the Teensy VUSB-VIN trace.** Otherwise the 5 V rail and the USB host
  fight each other whenever a laptop is plugged in.
- One star ground at the supply. Motor return currents must not share a
  conductor with encoder or analog ground, or the current-sense readings will
  move whenever the motors do.
- Electrolytic across each driver's supply terminals, as close as the layout
  allows. The 24 V rail sags hard on a direction reversal.
- The e-stop breaks the **driver supply**, not the logic supply. The Teensy
  stays alive, sees the loop open and reports the fault; the motors are dead
  regardless of what the firmware does.

## Connectors

Anything that crosses a joint gets a locking connector and a service loop.
The cable that will eventually fail is the one that flexes every cycle, so
plan for replacing it rather than hoping.
