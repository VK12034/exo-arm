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
| Driven past the elbow's working range | Planner decelerates to rest at the soft limit; step generator refuses to pulse past it independently; overrun switch three degrees past the flexion limit faults the arm if both of those fail. Mechanical hard stops behind all three are assumed throughout this page but are **not in the CAD** — see Known gaps |
| Runaway from a firmware fault | E-stop breaks the driver supply in hardware, not through the firmware |
| Motion on power-up from a stuck button | Firmware boots into fault and will not leave it until the e-stop is closed and both buttons are released |
| Accumulated step loss moving the limits | Home switch at the extension end re-references the counter every time the arm reaches it; a disagreement beyond 5° faults rather than being corrected quietly. Re-zero gesture as a fallback |
| Travel limits trusted before anything has measured them | Until the home switch is reached the arm is held to 90° at 5°/s, so a parking error cannot put the elbow somewhere it should not be |
| Ambiguous command | Both buttons, or neither, reads as stop |
| Arm binding or pushing against something | Supply current monitored while driving; sustained overload faults the arm. Partial — see Known gaps |
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

Homing does not break this, and that shaped how it works. The obvious way to
find zero is to drive the arm at the switch on power-up, and that was
rejected: it would mean the machine moving on its own to fix a lesser
problem. Instead the zero is adopted the moment the wearer extends the arm
onto the switch, which is a motion they were commanding anyway. The cost is
that the arm runs restricted until they do. That is the right trade.

## Known gaps

Stated plainly, because a safety page that only lists mitigations is a sales
page:

- **Load monitoring is inferred, not measured.** A TB6600 regulates phase
  current, so the motor draws much the same whether or not it is turning.
  What the firmware watches is the *supply* current, which tracks mechanical
  power: it climbs as the arm works harder and collapses once the rotor loses
  synchronism. Sustained overload faults the arm, and so does a collapse
  while steps are being commanded.

  Both are heuristics. A slow bind that never crosses the threshold will not
  trip, a collapse reads the same as a disconnected motor, and a stall that
  happens below the overload threshold is invisible. A shaft encoder would
  make this exact instead of inferred, and remains the right fix.

- **Lost steps are detected late, not prevented.** The home switch gives the
  counter a way back to the truth, but only at one end of travel and only
  when the arm is taken there. Between one homing and the next, loss
  accumulates unseen exactly as before, and the flexion limit moves with it.
  What has changed is that the error is now caught and measured instead of
  compounding forever, and that a large one faults rather than being
  silently corrected.
- **Both ends now have a switch, but they do different jobs.** Zero is
  measured at the home switch every time the wearer extends onto it. The
  flexion end has an overrun switch three degrees past the soft limit, which
  is a check on the firmware rather than a limit: reaching it means the
  planner and the step clamp both failed. What there is still no physical
  reference for is the soft limit itself, at 135°, which remains arithmetic.
- **The overrun switch fails silently.** A broken home switch is obvious
  within a session, because the arm never homes and the status line says so.
  A broken overrun switch reads exactly like one that has never needed to
  fire. Nothing distinguishes them until it is needed, which is why step 7 of
  [commissioning.md](commissioning.md) exists and why it should be repeated
  after any work on the loom.
- **The home switch is a single contact with no cross-check.** It is wired
  normally open so that a break reads as "not home" and merely costs range
  (see [wiring.md](wiring.md)), and a contact jammed closed while the counter
  says the arm is well into flexion faults. Neither covers a switch that
  closes early because the lever is bent.
- **Drift is only detectable in one direction.** Lost steps leave the arm
  physically further extended than the counter claims, so it reaches the
  switch with the count still positive and the error is measured. The
  opposite error is invisible: the soft minimum halts the arm at a reported
  0° before the switch can be reached. That is the benign direction — it
  costs range rather than overrunning a limit — but it means a clean homing
  message is not proof the counter is right.
- **No torque limit, and the firmware cannot provide one.** The drivetrain
  can deliver roughly seven times the static load, and nothing stops that
  being applied to an arm.

  This one deserves spelling out, because the obvious software fix does not
  work. A TB6600 regulates phase current to whatever its DIP switches are
  set to, and a stepper's torque follows its phase current. So the force the
  motor can push with is fixed in hardware. It does not fall when the
  firmware commands a lower speed, and slowing down does not ease off
  against an obstruction: stepper torque rises as speed drops, so a slow
  command pushes at least as hard as a fast one.

  Which means there is no software lever between "stepping" and "stopped".
  The overload fault is a trip, not a limit: it detects that too much force
  is already being applied and cuts the drive. Everything up to the
  threshold, for as long as the grace period lasts, goes into the wearer.

  The real fixes, none of them firmware:

  - **A mechanical torque limiter** between the worm output and the forearm,
    sized to slip below the injury threshold. This is the standard answer for
    a non-backdrivable actuator worn by a person, and it is the only one that
    works when the firmware is wrong, which is the case that matters.
  - **A lower driver current setting.** The DIP switches trade peak torque
    for capability directly, and seven times the static load is far more than
    this machine needs. See step 10 of [commissioning.md](commissioning.md).
    Cheap, available now, and partial: it lowers the ceiling rather than
    limiting the force below it.
  - **A driver whose current is software-settable**, which would make a real
    limit possible and is a different machine from this one.
- **The physical backstops do not exist yet.** This page has always treated
  mechanical hard stops as the last line behind the soft limits, and the home
  switch now assumes one to be sited against. Neither is in the Fusion model:
  of its 82 bodies none is a travel stop, and none is a mount for the switch.

  What is real is the firmware: the planner decelerates to rest at the soft
  limit, and the step generator refuses to pulse past it on its own
  arithmetic. Those are tested. What is missing is the thing that catches
  them both when they are wrong, which is exactly the case they exist for.
  Until the stops are designed and printed, a firmware fault that defeats
  both limits has nothing physical in its way, and the e-stop in the free
  hand is the only remaining layer.

  This is the most significant gap on the page. Do not read the three-layer
  description above as describing a machine that exists.
- **Never tested on hardware.** Every number in `config.h` is from CAD or
  from a datasheet, not from a bench.

None of this is a substitute for the e-stop being in the wearer's free hand,
or for somebody else being present. The load monitor infers rather than
measures, the counter is only referenced at one end of travel, and nothing
here has been run against hardware.

## Before every session

- [ ] E-stop in the free hand, and a second person who knows where the supply
      switch is
- [ ] Confirm the mechanical hard stops are actually fitted. They are not in
      the CAD, so on an early build they are probably not there
- [ ] Bench test with the arm **not worn**: drive to both soft limits and
      confirm it stops short of the hard stops
- [ ] Press the e-stop mid-motion and confirm the motor goes dead
- [ ] Release both buttons mid-motion and confirm it ramps to a stop
- [ ] Check the forearm cuff, upper cuff, sling and belt, and the output hub
      grub screw on the keyed shaft
- [ ] Confirm the arm is parked at full extension before power-on, and that
      the monitor prints `homed:` rather than leaving `zero=UNHOMED`
- [ ] Read the power-on log. Any fault from the last session is in it, and so
      is the drift trend. Degrees of correction, repeatedly, means the machine
      is losing steps

## First time it is worn

Supply current limited at the bench supply. Straps loose enough to pull free.
Someone else holding the e-stop. Move through the range in short presses, not
one long one, and stop at the first thing that feels wrong.

## Limits are set conservatively

`kMaxAngleDeg` is 135°, inside the elbow's anatomical range and inside where
the mechanism's hard stops are intended to go once they exist. If the arm
reaches a hard stop in normal use, the limits are wrong or the arm was parked
wrong. Do not widen the limits to make the symptom go away.

And while the stops are still unbuilt, the 135° figure is the only thing
bounding flexion at that end. Treat it as load-bearing rather than as a
conservative margin inside something sturdier.
