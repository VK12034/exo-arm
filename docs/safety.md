# Safety

**This is an amateur build. It is not a medical device, it meets no standard,
nobody qualified has assessed it, and the firmware has never run against
hardware. It is a powered machine strapped to a human arm, and it can injure
the person wearing it.**

Do not use it on anyone who cannot remove it themselves, cannot speak, or has
not read this page.

## What makes this machine dangerous

Two things, and both come from the drivetrain.

**Whether it holds itself is unproven.** Motovario's catalogue lists the NMRV030 at
30:1 with a static efficiency of 0.39, below the 0.5 it requires for a load not to turn
the worm, but warns that vibration and shocks can affect this anyway, and the gearbox
you buy may come from another maker. Until the holding test passes, assume losing power
or pressing the e-stop can let the forearm move under its own weight or the wearer's,
though the assist cord pulls the other way. A moving arm may also run on before it
stops. A failed chain releases the forearm whatever the worm does. See
[loads.md](loads.md).

**The wearer cannot rely on pushing back.** Through 60:1 the motor can produce
4 to 7 times the static load at its rated current, or about 65 to 110 N at the cuff. A
worm that holds also cannot be pushed back by hand, even with the power off. A command
error ends when the motor stops,
the e-stop opens, or the elbow reaches a limit, and stopping the motor may bring
the first problem with it.

Do not wear this arm until step 9 of [commissioning.md](commissioning.md) has
shown what happens when power is lost under load.

## Hazards and what is done about each

| Hazard | Mitigation |
|---|---|
| Driven past the elbow's working range | Planner decelerates to rest at the soft limit; step generator refuses to pulse past it independently; overrun switch three degrees past the flexion limit faults the arm if both of those fail; steel hard stops behind all three, modelled but not load-checked |
| Runaway from a firmware fault | E-stop breaks the driver supply in hardware, not through the firmware |
| Motion on power-up from a stuck button | Firmware boots into fault and will not leave it until the e-stop is closed and both buttons are released |
| Accumulated step loss moving the limits | Home switch at the extension end re-references the counter every time the arm reaches it; a disagreement beyond 5° faults rather than being corrected quietly. Re-zero gesture as a fallback |
| Travel limits trusted before anything has measured them | Until the home switch is reached the arm is held to 60° at 5°/s, so a parking error of up to about 30° stays short of the flexion stop |
| Ambiguous command | Both buttons, or neither, reads as stop |
| Arm binding or pushing against something | Supply current monitored while driving; sustained overload faults the arm. Partial — see Known gaps |
| Forearm moving when power is lost | **Should hold by catalogue figures, unproven.** Worm chosen to be statically irreversible. Motor also stays energised when idle and through every fault except the e-stop, but the e-stop and any power failure still remove current. See commissioning step 9 |
| Chain failure or skipped tooth | **Detected, not prevented.** Releases the forearm from the drive. The encoder sees the disagreement and faults, but cannot hold the arm |
| Lost steps or back-driving | Encoder on the elbow compares the measured angle with the count, stopped or moving, and faults on a sustained disagreement |
| Fingers, hair or clothing in the chain and sprockets | Printed guard in the model, not yet built. Its open end is roofed by the gearbox plate, so check for gaps on the real parts |
| Wearer trapped in a dead arm | **More likely if the worm holds, since the arm then locks where it stopped.** Every strap releases one-handed from the free hand; cuffs are quick-release |

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
  trip, and a collapse reads the same as a disconnected motor. The encoder now
  catches the lost motion that follows a stall, but it measures position, not force.
  Force on the wearer below the overload threshold remains invisible.

- **Lost steps are detected, not prevented.** The encoder catches a disagreement of
  more than 3° after 400 ms, and the home switch corrects smaller drift whenever the arm
  reaches it. Both react after motion has already been lost. If the encoder is lost the
  arm faults once and carries on step-counted.
- **Faults leave torque on.** Keeping the motor energised means an overload fault
  holds whatever force it had reached until someone presses the e-stop, and the e-stop
  may let the forearm drop. There is no state that does both safely, so a person has
  to choose, which is another reason someone else should be present.
- **Both ends now have a switch, but they do different jobs.** Zero is
  measured at the home switch every time the wearer extends onto it. The
  flexion end has an overrun switch three degrees past the soft limit, which
  is a check on the firmware rather than a limit: reaching it means the
  planner and the step clamp both failed. What there is still no physical
  reference for is the soft limit itself, at 88°, which remains arithmetic.
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
- **No torque limit below the motor's own, and the firmware cannot provide one.** The
  drivetrain's output is 12 to 21 N·m at the motor's rated current, against about
  2.9 N·m of static load, and nothing stops that being applied to an arm. A small
  motor lowers that ceiling but does not make it safe: about 110 N at the cuff is
  still enough to hurt a pinned arm.

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

  - **A mechanical torque limiter** between the drive and the forearm,
    sized to slip below the injury threshold. This is the standard answer for
    a stiff geared actuator worn by a person, and it is the only one that
    works when the firmware is wrong, which is the case that matters.
  - **A lower driver current setting.** The DIP switches trade peak torque
    for capability directly. [loads.md](loads.md) suggests starting at 2.0 A.
    See step 10 of [commissioning.md](commissioning.md).
    Cheap, available now, and partial: it lowers the ceiling rather than
    limiting the force below it. A wrong switch can raise it again, to about
    137 N at the cuff at the driver's 3.5 A top setting if torque keeps rising
    with current.
  - **A driver whose current is software-settable**, which would make a real
    limit possible and is a different machine from this one.
- **The hard stops are modelled but not proven.** Rubber-sleeved steel spacers on the
  upper strut stop the forearm strut at about 21° and 118° of true flexion. At the
  motor's rated current they could see 510 to 870 N, and the M5 bolt in bending, if the
  clamp does not help, reaches its limit at about 1400 N. See [loads.md](loads.md).
- **Switch closing angles are modelled targets.** Real levers differ, so
  commissioning measures them. Until then the margins around both switches are
  estimates.
- **Never tested on hardware.** Every number in `config.h` is from CAD or
  from a datasheet, not from a bench.

None of this is a substitute for the e-stop being in the wearer's free hand,
or for somebody else being present. The load monitor infers rather than
measures, the encoder reacts after motion is lost, and nothing here has been run
against hardware.

## Before every session

- [ ] E-stop in the free hand, and a second person who knows where the supply
      switch is
- [ ] Confirm the hard stops are fitted and their bolts are tight
- [ ] Confirm the chain guard is fitted and its screws are tight, and check chain
      tension and the sprockets
- [ ] Confirm the status line shows `enc=` with a reading rather than `LOST`
- [ ] Bench test with the arm **not worn**: drive to both soft limits and
      confirm it stops short of the hard stops
- [ ] Press the e-stop mid-motion and confirm the motor goes dead
- [ ] Release both buttons mid-motion and confirm it ramps to a stop
- [ ] Check the forearm cuff, upper cuff, sling and belt, the keyed sprocket on
      the gearbox output, and the elbow pivot bolt
- [ ] Confirm the arm is parked at the extension stop before power-on, and that
      the monitor prints `homed:` rather than leaving `zero=UNHOMED`
- [ ] Confirm the driver current is at the setting chosen in commissioning
- [ ] Read the power-on log. Any fault from the last session is in it, and so
      is the drift trend. Degrees of correction, repeatedly, means the machine
      is losing steps

## First time it is worn

Supply current limited at the bench supply. Straps loose enough to pull free.
Someone else holding the e-stop. Move through the range in short presses, not
one long one, and stop at the first thing that feels wrong.

## Limits are set conservatively

`kMaxAngleDeg` is 88° from the home switch, about 111.5° of true flexion, and
about 6° short of the flexion stop. If the arm reaches a hard stop in normal use,
the limits are wrong or the arm was parked wrong. Do not widen the limits to make
the symptom go away.
