# Safety

**This is an amateur build. It is not a medical device, it is not certified,
and it has not been assessed by anyone qualified to assess it. It is a
powered machine strapped to a human arm, and it can injure the person wearing
it.**

Do not use it on anyone who cannot remove it themselves, cannot speak, or has
not read this page.

## Hazards and what is done about each

| Hazard | Mitigation |
|---|---|
| Joint driven past the anatomical range | Soft limits in firmware, command ramped to zero inside the margin, mechanical hard stops behind both |
| Runaway from a firmware or host fault | Normally-closed e-stop loop in the driver sleep path, host watchdog, deadman switch |
| Motor stall burning out a winding or the wearer | Per-joint current ceiling with proportional cutback, then a latched fault |
| Sudden motion on enable | Target initialises to the measured pose; duty slew limiter; PID state reset on every enable |
| Encoder slip or disconnection | Position jump shows as an implausible velocity; the arm faults rather than chasing a bad reading |
| Wearer trapped in a dead arm | Every strap releases one-handed from the free hand; the arm is backdrivable with the drivers asleep |

## Interlocks

Three independent things must all be true before any joint drives:

1. **E-stop loop closed.** Normally closed to ground. A pressed button, a
   pulled connector and a cut wire all read the same: open, meaning stop.
2. **Deadman held.** A grip switch in the free hand. Release it and the arm
   drops to idle.
3. **No latched fault,** and the mode is not IDLE.

Any fault latches. The host must clear it explicitly, and
Safety::clearFaults() refuses while the cause is still present, so a held
e-stop cannot be cleared in a loop.

## Before every session

- [ ] E-stop reachable by the free hand and by anyone else in the room
- [ ] Bench test with the arm **not worn**: step each joint, confirm it stops
      at both soft limits
- [ ] Press the e-stop mid-motion and confirm both joints go dead
- [ ] Release the deadman mid-motion and confirm the same
- [ ] Unplug the host mid-motion and confirm the watchdog fires within 500 ms
- [ ] Check every strap and fastener, and the gearbox output couplings
- [ ] A second person present who knows where the supply switch is

## First time it is worn

Supply current limited to 2 A, ASSIST mode, gains at half the values in
config.h. Someone else holds the e-stop. Move slowly through the full range
with the straps loose enough to pull free.

## Limits are set conservatively

The soft limits in config.h are well inside the mechanical hard stops, and
the hard stops are set inside the wearer's comfortable range, not their
maximum range. If a joint reaches a hard stop in normal use, the limits are
wrong; do not widen them to make the symptom go away.
