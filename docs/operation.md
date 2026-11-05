# Operation

## Power-on

There is no encoder. The firmware counts steps, and that count only means
something once it has been referenced to the home switch at full extension.

1. **Park the arm at full extension**, against its stop, before switching on.
2. Switch on. The status LED blinks fast: the firmware boots into fault.
3. Release the e-stop mushroom if it is latched, and let go of both buttons.
   The LED drops to a slow blink and the monitor prints `ready`.

If the arm was parked properly it is already on the home switch, and the
monitor prints this on the first pass:

```
homed: zero set at the extension switch
```

That is the normal case and there is nothing else to do.

## If it does not home

An arm that is not on the switch at power-on runs **unhomed**, which the
status line reports as `zero=UNHOMED`. In that state the firmware does not
pretend to know where the arm is, so it holds back:

| | Unhomed | Homed |
|---|---|---|
| Speed | 5°/s | 20°/s |
| Flexion limit | 90° | 135° |
| Extension | free, seeking the switch | stops at 0° |

Hold **DOWN** and let the arm extend. The moment it reaches the switch the
zero is adopted, full travel and full speed come back, and the message above
prints. This is the homing routine: there isn't a separate one, because the
arm never moves unless you are holding a button.

So an unhomed arm is slow and short of range, not unsafe. If it never homes
however far you extend it, the switch or its wiring is the problem, and
`home=` in the status line will be stuck at `open`.

## The power-on log

The arm keeps a short history in EEPROM and prints it as part of the startup
banner:

```
log: session 14, 3 events, oldest first:
  s11 drift 41 steps (0.62 deg)
  s12 overload at 73.4 deg
  s13 drift 96 steps (1.44 deg)
```

`session` counts power-ons. Each line is tagged with the session it happened
in, so the gaps tell you as much as the entries.

| Entry | Meaning |
|---|---|
| `drift N steps` | A homing correction of more than half a degree. The trend across sessions is the point |
| `overload at X deg` | The arm faulted pushing against something, at that angle |
| `current collapse at X deg` | Stall or disconnected motor fault, at that angle |
| `home switch disagreed, counter read X deg` | More than 5° of lost steps found at the switch |
| `soft limit overrun, span measured N steps` | The flexion switch was reached, so both soft limits failed. The span is also a microstepping check |
| `switch closed implausibly, counter read X deg` | A switch closed where it physically cannot be. Wiring |

Routine e-stop presses are **not** logged. They are normal operation and
would crowd out the entries that matter.

The last 16 events are kept; older ones fall off. Reading the log at power-on
is the only way to get at it, by design: a serial command to dump it would
mean this firmware accepting serial input, and the parser is the thing worth
not having. See [design.md](design.md).

`log: session 1, initialised empty` on a board you have used before means the
EEPROM was wiped or the format changed. The history is gone; the arm is fine.

## Status LED

| Pattern | Meaning |
|---|---|
| Fast blink, about 4 Hz | Fault: e-stop open, or waiting for the buttons to be released |
| Slow blink, about 0.6 Hz | Ready, motor idle |
| Solid | Moving |

## The two switches

They look alike and do unrelated jobs.

| | Home switch | Overrun switch |
|---|---|---|
| Where | At full extension, 0° | Past the flexion limit, about 138° |
| Reached | Every session, on purpose | Never, if everything is working |
| On contact | Sets zero, restores full travel | Faults the arm |
| Status field | `home=` | `flex=` |
| If it breaks | Obvious: the arm never homes | Invisible until it is needed |

`flex=CLOSED` in normal use is not a thing that should ever happen. If you see
it, stop.

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

If it sags when the motor drops out, stop using it and re-read step 9 of
[commissioning.md](commissioning.md).

## Re-zeroing

Steps can still be lost — against an obstruction, at a hard stop, or from a
stall. The difference the home switch makes is that the error gets caught and
corrected instead of accumulating silently.

**The normal fix is to extend to the switch.** Every time the arm reaches it
the count is re-referenced. If the correction was more than half a degree the
monitor says how big it was:

```
homed: zero set at the extension switch, counter was out by 164 steps (2.46 deg)
```

Worth watching. A couple of steps is rounding. Degrees, session after
session, means the machine is losing steps and the cause is mechanical.

Past five degrees the firmware faults instead of correcting:

```
FAULT: home switch disagrees with the step counter - steps lost
```

That much loss means the flexion limit was also wrong by that much while the
arm was being used, so it drops back to unhomed and restricted travel rather
than quietly carrying on. Extend to the switch again to re-home.

### Asserting a zero by hand

For a build with no switch fitted, or to recover a session without power
cycling: hold **both buttons for three seconds** with the arm stopped. The
current pose becomes 0° and full travel is restored.

```
rezeroed: this pose is asserted to be full extension
```

The status line then reads `zero=asserted` rather than `zero=switch`, because
this one is your claim rather than a measurement. Only do it with the arm
genuinely at full extension: it moves both soft limits by whatever the error
is, and the arm will then drive into a hard stop.

**If you get it wrong, power cycle.** Asserting a zero puts the soft minimum
at that pose, so an arm that was actually flexed can no longer extend — it is
already at its reported 0° and will refuse to go further. There is no gesture
out of that, by design: the alternative was removing the soft minimum from
exactly the builds that have no switch to fall back on. Switching off and on
comes up unhomed, which is free to seek again.

## Serial status

115200 baud, a line every half second:

```
t=12480 state=MOVING angle=42.3deg steps=2820 v=1333 estop=closed home=open flex=open zero=switch amps=1.18 drive=on
```

| Field | Meaning |
|---|---|
| `state` | `FAULT`, `IDLE` or `MOVING` |
| `angle` | Degrees of flexion from zero |
| `steps` | Raw step count |
| `v` | Commanded velocity, steps per second |
| `estop` | `OPEN` or `closed` |
| `home` | Home switch contact: `closed` means the arm is on it |
| `flex` | Overrun switch. Should always read `open`; `CLOSED` is shouted because it means trouble |
| `zero` | `switch` if measured, `asserted` if set by hand, `UNHOMED` if neither |
| `amps` | Filtered supply current to the driver |
| `drive` | Whether the motor is energised |

`zero=UNHOMED` is the one to notice: the arm is on restricted travel until it
reaches the switch.

## Faults

| Symptom | Cause |
|---|---|
| Fast blink at power-on | Normal. Release the e-stop and both buttons |
| Fast blink that will not clear | E-stop loop open: mushroom latched, connector out, or cable broken |
| `ready` never prints | A button is stuck closed |
| Angle drifts from reality | Steps lost. Extend to the home switch to re-reference |
| `zero=UNHOMED` persists | Arm has not reached the switch, or the switch is not wired. Check `home=` |
| `FAULT: home switch disagrees` | More than 5° of lost steps. Re-home and find out why |
| `FAULT: flexion switch reached` | Both soft limits were passed. This is a firmware or configuration fault, not wear. Stop and find out why before using the arm |
| `FAULT: flexion switch closed with the counter near zero` | Shorted switch, wrong pin, or the arm was parked at full flexion |
| Buzz without motion | Stall. Reduce speed, check for a mechanical bind, check driver current |
| `FAULT: overload` | The arm is working against something. Find out what before clearing it |
| `FAULT: current collapse` | Motor disconnected, or the rotor lost synchronism. Check the motor cable first |
| Arm sags when idle | The worm is not holding. Stop using it |

## After a load fault

Both load faults clear the same way as an e-stop: release both buttons and
the arm returns to ready. That is deliberately easy, because being stuck in a
fault with an arm strapped on is its own hazard.

It is also the wrong thing to do twice. A repeated overload means the arm is
binding on something, and the second attempt pushes into it harder than the
first. Take the arm off and find the cause.

After either fault, treat the reported angle as suspect: steps may have been
lost. Extend to the home switch before relying on the limits again, and note
the size of the correction it reports.

## Shutdown

Drive to full extension, release the buttons, let it come to rest, then
switch off. Leaving it parked on the home switch means the next power-on
homes immediately instead of starting out slow and short of range.
