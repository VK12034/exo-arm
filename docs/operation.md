# Operation

## Power-on

There is no encoder. The firmware's only idea of where the arm is comes from
counting steps away from the pose it was in when power arrived.

1. **Park the arm at full extension**, against its stop, before switching on.
2. Switch on. The status LED blinks fast: the firmware boots into fault.
3. Release the e-stop mushroom if it is latched, and let go of both buttons.
   The LED drops to a slow blink and the monitor prints `ready`.

If the arm was not parked, every angle is wrong by the same offset and the
soft limits move with it. Power cycle and park it properly.

## Status LED

| Pattern | Meaning |
|---|---|
| Fast blink, about 4 Hz | Fault: e-stop open, or waiting for the buttons to be released |
| Slow blink, about 0.6 Hz | Ready, motor idle |
| Solid | Moving |

## Moving the arm

| Input | Effect |
|---|---|
| Hold UP | Elbow flexes, ramping to 20°/s |
| Hold DOWN | Elbow extends |
| Release | Ramps to a stop |
| Both together | Stops |
| E-stop | Motor dead immediately |

Motion only happens while a button is held. There is no go-to-angle command
and no serial control; the serial port is status output only.

Near a travel limit the arm slows and comes to rest by itself. That is the
planner decelerating, not a fault — the arm will still drive away from the
limit the moment you press the other button.

## After a stop

The motor de-energises three seconds after the arm stops moving. The elbow
holds its angle anyway: the worm is self-locking. Expect the arm to go quiet
and stay exactly where it was.

If it sags when the motor drops out, stop using it and re-read step 7 of
[commissioning.md](commissioning.md).

## Re-zeroing

Steps can be lost — against an obstruction, at a hard stop, or from a stall
— and nothing corrects for it. Over time the reported angle drifts from the
real one, which also moves the soft limits.

To fix it: park the arm at full extension and power cycle. That is the clean
way.

The alternative, for when a power cycle is inconvenient: hold **both buttons
for three seconds** with the arm stopped. The current pose becomes 0°. The
monitor prints:

```
rezeroed: this pose is now full extension
```

Only do this with the arm genuinely at full extension. Re-zeroing anywhere
else moves both soft limits by that error, and the arm will then happily
drive into a hard stop.

## Serial status

115200 baud, a line every half second:

```
t=12480 state=MOVING angle=42.3deg steps=2820 v=1333 estop=closed drive=on
```

| Field | Meaning |
|---|---|
| `state` | `FAULT`, `IDLE` or `MOVING` |
| `angle` | Degrees of flexion from the power-on pose |
| `steps` | Raw step count |
| `v` | Commanded velocity, steps per second |
| `estop` | `OPEN` or `closed` |
| `drive` | Whether the motor is energised |

## Faults

| Symptom | Cause |
|---|---|
| Fast blink at power-on | Normal. Release the e-stop and both buttons |
| Fast blink that will not clear | E-stop loop open: mushroom latched, connector out, or cable broken |
| `ready` never prints | A button is stuck closed |
| Angle drifts from reality | Steps lost. Park and power cycle |
| Buzz without motion | Stall. Reduce speed, check for a mechanical bind, check driver current |
| Arm sags when idle | The worm is not holding. Stop using it |

## Shutdown

Drive to full extension, release the buttons, let it come to rest, then
switch off. Leaving it parked at the extension stop means the next power-on
starts from a pose the firmware's assumption actually matches.
