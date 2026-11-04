# Safety

**This is an amateur build. It is not a medical device, it meets no standard,
nobody qualified has assessed it, and the firmware has never run against
hardware. It is a powered machine strapped to a human arm, and it can injure
the person wearing it.**

Do not use it on anyone who cannot remove it themselves, cannot speak, or has
not read this page.

## What makes this machine dangerous

The worm drive is self-locking. That is the feature which makes the arm hold
its pose without power, and it is also the hazard: **the wearer cannot push
back**. On a backdrivable machine, a command error ends with the wearer
overpowering it. Here, a command error ends when the motor stops, the e-stop
opens, or the elbow reaches a limit — whichever comes first.

Everything below follows from that.

## Hazards and what is done about each

| Hazard | Mitigation |
|---|---|
| Driven past the elbow's working range | Planner decelerates to rest at the soft limit; step generator refuses to pulse past it independently; mechanical hard stops behind both |
| Runaway from a firmware fault | E-stop breaks the driver supply in hardware, not through the firmware |
| Motion on power-up from a stuck button | Firmware boots into fault and will not leave it until the e-stop is closed and both buttons are released |
| Accumulated step loss moving the limits | Re-zero gesture, and a power-on park against the extension stop |
| Ambiguous command | Both buttons, or neither, reads as stop |
| Stepper stalling against an obstruction | **Not mitigated.** See Known gaps |
| Wearer trapped in a dead arm | Every strap releases one-handed from the free hand; cuffs are quick-release |

## Interlocks

Two independent things must be true before the elbow moves:

1. **E-stop loop closed.** Normally closed to ground. A pressed mushroom, an
   unplugged pendant and a cut cable all read the same: open, meaning stop.
   It breaks the driver supply as well as being read by the firmware.
2. **Exactly one button held.** Release it and the arm ramps to a stop.

There is no software path that moves the arm without a button held. There is
no "go to angle" command, no serial control, and no autonomous motion. This
is deliberate: the only motion this machine performs is motion a human is
actively asking for, for as long as they are asking for it.

## Known gaps

Stated plainly, because a safety page that only lists mitigations is a sales
page:

- **No stall detection.** If the arm jams, the motor keeps stepping, the
  position counter keeps counting, and the firmware believes the arm is
  somewhere it is not. The limits are then wrong by that error until the next
  re-zero.
- **No limit switches.** Soft limits are counted steps from an assumed
  power-on pose. Park the arm wrong and the limits are wrong with it.
- **No torque limit.** The drivetrain can deliver roughly seven times the
  static load. Nothing in the firmware prevents it being applied to an arm.
- **Never tested on hardware.** Every number in `config.h` is from CAD or
  from a datasheet, not from a bench.

The first three are why the e-stop must be in the wearer's free hand and why
somebody else must be present.

## Before every session

- [ ] E-stop in the free hand, and a second person who knows where the supply
      switch is
- [ ] Bench test with the arm **not worn**: drive to both soft limits and
      confirm it stops short of the hard stops
- [ ] Press the e-stop mid-motion and confirm the motor goes dead
- [ ] Release both buttons mid-motion and confirm it ramps to a stop
- [ ] Check the forearm cuff, upper cuff, sling and belt, and the output hub
      grub screw on the keyed shaft
- [ ] Confirm the arm is parked at full extension before power-on

## First time it is worn

Supply current limited at the bench supply. Straps loose enough to pull free.
Someone else holding the e-stop. Move through the range in short presses, not
one long one, and stop at the first thing that feels wrong.

## Limits are set conservatively

`kMaxAngleDeg` is 135°, inside both the elbow's anatomical range and the
mechanism's hard stops. If the arm reaches a hard stop in normal use, the
limits are wrong or the arm was parked wrong. Do not widen the limits to make
the symptom go away.
