# Operation

## Power-on

The arm has incremental encoders only, so it has no idea where it is until it
is told. The startup ritual is the telling.

1. **Park the arm against its lower hard stops**, shoulder down and elbow
   straight, before applying power.
2. Power on. The firmware zeroes each joint at kMinAngle.
3. The status LED blinks slowly: idle, no faults.
4. Connect the host and confirm telemetry:

        exoarm --port COM5 status

If the arm was not parked, every angle is wrong by the same offset, and the
soft limits are wrong with them. Power cycle and do it again.

## Status LED

| Pattern | Meaning |
|---|---|
| Slow blink, 1 Hz | Idle, or waiting on an interlock |
| Solid | Driving |
| Fast blink, 5 Hz | Latched fault |

## Enabling

    exoarm --port COM5 mode hold     # or assist

Nothing drives until the deadman is held. Holding it is what enables the
joints; releasing it idles them. There is no software equivalent, on purpose.

## Faults

    exoarm --port COM5 status        # read the fault flags
    exoarm --port COM5 clear         # refuses while the cause is still present

| Fault | Usual cause |
|---|---|
| ESTOP | Button pressed, or the loop is broken somewhere |
| OVER_CURRENT | Mechanical bind, or a gain set too high |
| DRIVER | Driver over-temperature or under-voltage. Check the supply sag |
| LIMIT | A joint got outside its soft limits, usually a bad power-on park |
| HOST_TIMEOUT | USB disconnected or the host process died |
| ENCODER | Implausible position jump. Check the encoder cable |

## Shutdown

Idle the arm, release the deadman, then cut the driver supply before the
logic supply. Park it against the lower stops so the next power-on starts
from a known pose.
