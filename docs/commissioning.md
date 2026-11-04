# Commissioning

Work through this in order, with the arm **on the bench and not worn**, until
the last step. The firmware has never run on hardware, so treat every step as
a test of the firmware as much as of the build.

Keep the supply current-limited throughout. A limit of about 1 A is enough to
move the arm unloaded and not enough to do damage when something is wrong.

## 1. Driver alone, motor disconnected

Flash the firmware and open the monitor:

```bash
cd firmware
pio run -e uno -t upload
pio device monitor
```

Expect, at power-on:

```
exo-arm elbow ready
travel 0-135 deg, 66.7 steps/deg
```

and then nothing until the e-stop reads closed, because the firmware boots
into fault on purpose.

- Status LED blinking fast: fault. Close the e-stop loop.
- `ready` printed and the LED blinking slowly: the interlocks are working.

If it prints `ESTOP` with the mushroom released, the loop is wired
normally-open. Fix that before going further — a normally-open e-stop reads
as safe when the cable is cut.

## 2. Buttons

With the motor still disconnected, press each button and watch the status
line. UP should show a positive `v=`, DOWN negative, both together zero.

If UP and DOWN are swapped, swap the wires rather than the pin numbers — the
label on the pendant is what the wearer trusts.

## 3. Direction, motor connected, arm not attached

**This is the step that hurts someone if it is wrong.** Connect the motor but
leave the forearm module uncoupled from the output hub, so a wrong direction
turns a shaft instead of an arm.

Press UP. The output shaft must turn in the direction that would **flex** the
elbow. If it turns the other way, set `kInvertDirection` to `true` in
`firmware/include/config.h` and reflash.

Do not fix this by swapping motor phases: the firmware's idea of direction
and the physical direction must agree, and the constant is where that is
recorded.

## 4. Steps per degree

Still uncoupled. Mark the output shaft, hold UP until the status line reads
roughly 90°, and measure the actual shaft rotation with a protractor.

| Reading | Meaning |
|---|---|
| Shaft moved ≈ 90° | `kMicrosteps` and `kGearRatio` agree with the hardware |
| Shaft moved ≈ 45° | Driver is set to 1/16, firmware expects 1/8 |
| Shaft moved ≈ 180° | Driver is set to 1/4 |

The DIP switches on the TB6600 and `cfg::kMicrosteps` must match. Nothing in
the firmware can detect a mismatch, and every angle, limit and speed is wrong
by the same factor when they disagree.

## 5. Travel limits

Couple the forearm module. Park at full extension, power cycle, then hold UP
and let it run to the limit without touching anything.

- It must come to a **stop**, not a halt: the last part of the travel is a
  deceleration ramp.
- It must stop short of the mechanical hard stop.
- The status line must read close to 135°.

Repeat with DOWN to the extension limit.

If it reaches a hard stop, stop and work out why before continuing. Either
the park was wrong, the steps-per-degree is wrong, or steps are being lost.

## 6. Interlocks under motion

Each of these, with the arm moving:

- [ ] Press the e-stop. The motor must go dead immediately, and the monitor
      print `ESTOP`.
- [ ] Release the e-stop. The arm must **not** resume, even with a button
      still held. It should print `ready` only once both buttons are up.
- [ ] Release the button. The arm must ramp down, not stop dead.
- [ ] Press both buttons together. The arm must stop.
- [ ] Pull the pendant connector. Same behaviour as the e-stop.

The last one is the test people skip. A severed pendant cable must read as
stop, and that is only true if the e-stop contact is normally closed.

## 7. Idle behaviour

Leave the arm stopped mid-travel for five seconds. The motor should go quiet
and the holding current drop away, and **the elbow must not sag**. If it
sags, the worm is not holding and the whole safety argument for this design
does not apply to your gearbox. Stop and reconsider the ratio.

## 8. Speed, loaded

With a representative weight in the cuff, run the full travel. 20°/s should
feel unhurried. If the motor stalls or skips — a buzz without motion — reduce
`kMaxSpeedDegPerSec` and `kAccelDegPerSec2` before raising the driver
current.

Record what the supply draws, from the `amps=` field in the status line:

| Condition | Figure to record |
|---|---|
| Stopped, driver de-energised | Should read near zero. If not, the zero calibration at boot caught something moving |
| Moving, unloaded | The floor the collapse threshold sits below |
| Moving, loaded | The normal working figure |
| Deliberately obstructed, briefly | The level the overload threshold sits below |

Then set the thresholds in `config.h`:

- `kCollapseAmps` about half the unloaded moving figure
- `kOverloadAmps` midway between the loaded working figure and the obstructed
  one

The values shipped are placeholders sized for a NEMA 23 at roughly 2 A per
phase. Set too low they will trip during normal use, and a safety device that
cries wolf gets disabled; set too high they catch nothing.

Obstruct the arm by hand against a bench, not against a person, and keep it
brief.

## 9. Load monitoring

With the thresholds set, confirm both trips fire:

- [ ] Obstruct the arm while it drives. It must fault within about half a
      second and print `FAULT: overload`
- [ ] Disconnect the motor and command motion. It must fault and print
      `FAULT: current collapse`
- [ ] Run the full travel loaded, several times, with no false trips

The third is the one that decides whether the thresholds are usable. A
monitor that trips during normal use will be turned off, and then it protects
nothing.

## 10. First wear

Follow the checklist in [safety.md](safety.md). Short presses, not long ones.
Someone else on the e-stop.
