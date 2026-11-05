"""First-pass sizing of the friction torque limiter at the 30T sprocket.

Run with: python tools/clutch_sizing.py

Geometry comes from cad/BudgetExoArm.design.json. The friction coefficient is a
guess, so these figures size the springs and show how far the nuts must travel.
The slip torque itself is set by measuring it on the built arm: see
docs/torque-limiter.md.
"""

import math

# --- Target ---
SLIP_TORQUE_NM = 8.0
STATIC_LOAD_NM = 2.9  # tools/stall_loads.py
CUFF_RADIUS_M = 0.193
STOP_LEVER_M = 0.0243
STOP_BOLT_LIMIT_NM = 34.2  # tools/stall_loads.py, M5 in bending
MOTOR_AT_START_NM = (8.9, 15.1)  # elbow, driver at 2.0 A, tools/stall_loads.py
CHAIN_RATIO = 2.0
CHAIN_EFFICIENCY = 0.97

# --- Clutch ---
FACING_OUTER_MM = 38.5
FACING_INNER_MM = 27.0
FRICTION_FACES = 2
BOLTS = 4
SPRINGS_PER_BOLT = 4
NUT_PITCH_MM = 0.8  # M5
FRICTION_RANGE = (0.15, 0.25, 0.40)  # assumed, 0.25 is the nominal figure
WEAR_GAP_MM = 0.5  # hub to outer plate, the most wear before clamping is lost

# --- DIN 2093 disc spring, 10 x 5.2 x 0.4, by the Almen-Laszlo formula ---
SPRING_OD_MM = 10.0
SPRING_ID_MM = 5.2
SPRING_T_MM = 0.4
SPRING_H0_MM = 0.3
E_MPA = 206e3
POISSON = 0.3


def mean_radius_m():
    outer, inner = FACING_OUTER_MM, FACING_INNER_MM
    return (2.0 / 3.0) * (outer**3 - inner**3) / (outer**2 - inner**2) / 1000.0


def spring_force(deflection, t=SPRING_T_MM, h0=SPRING_H0_MM, od=SPRING_OD_MM, id_=SPRING_ID_MM):
    """Force in N on one disc spring at a deflection in mm."""
    delta = od / id_
    k1 = (1.0 / math.pi) * ((delta - 1.0) / delta) ** 2 / (
        (delta + 1.0) / (delta - 1.0) - 2.0 / math.log(delta)
    )
    s = deflection / t
    return 4.0 * E_MPA / (1.0 - POISSON**2) * t**4 / (k1 * od**2) * s * (
        (h0 / t - s) * (h0 / t - s / 2.0) + 1.0
    )


def deflection_for(force):
    """Deflection in mm of one disc spring at a force in N, by bisection."""
    low, high = 0.0, SPRING_H0_MM
    for _ in range(60):
        middle = (low + high) / 2.0
        if spring_force(middle) < force:
            low = middle
        else:
            high = middle
    return (low + high) / 2.0


def main():
    radius = mean_radius_m()
    area = math.pi * (FACING_OUTER_MM**2 - FACING_INNER_MM**2)
    print("Facing mean radius %.1f mm, area %.0f mm2" % (radius * 1000.0, area))
    print("Spring check, 10 x 5.2 x 0.5: %.0f N at 0.188 mm, published 325 N"
          % spring_force(0.1875, 0.5, 0.25))
    print("One spring flat: %.0f N at %.2f mm" % (spring_force(SPRING_H0_MM), SPRING_H0_MM))
    print()

    print("At %.1f N.m slip:" % SLIP_TORQUE_NM)
    print("  Force at the cuff: %.0f N" % (SLIP_TORQUE_NM / CUFF_RADIUS_M))
    print("  Times the static load: %.1f" % (SLIP_TORQUE_NM / STATIC_LOAD_NM))
    print("  On a hard stop: %.0f N, %.1f times below the bolt's limit"
          % (SLIP_TORQUE_NM / STOP_LEVER_M, STOP_BOLT_LIMIT_NM / SLIP_TORQUE_NM))
    print("  Gearbox output: %.1f N.m" % (SLIP_TORQUE_NM / (CHAIN_RATIO * CHAIN_EFFICIENCY)))
    print("  Motor at the 2.0 A setting: %.1f to %.1f N.m at the elbow" % MOTOR_AT_START_NM)
    print("  Slip per bolt: %.0f N of shear at 20 mm" % (SLIP_TORQUE_NM / 0.020 / BOLTS))
    print()

    print("Friction | clamp | per bolt | pressure | stack travel | nut turns from flat-out")
    settings = {}
    for mu in FRICTION_RANGE:
        clamp = SLIP_TORQUE_NM / (FRICTION_FACES * mu * radius)
        per_bolt = clamp / BOLTS
        travel = deflection_for(per_bolt) * SPRINGS_PER_BOLT
        settings[mu] = travel
        print("  %.2f   | %4.0f N | %4.0f N   | %.2f MPa | %.2f mm      | %.2f"
              % (mu, clamp, per_bolt, clamp / area, travel, travel / NUT_PITCH_MM))
    print()

    travel = settings[0.25]
    print("Wear, set to %.1f N.m at friction 0.25:" % SLIP_TORQUE_NM)
    for wear in (0.05, 0.10, 0.20, 0.30):
        remaining = max(travel - wear, 0.0)
        per_bolt = spring_force(remaining / SPRINGS_PER_BOLT)
        torque = per_bolt * BOLTS * FRICTION_FACES * 0.25 * radius
        print("  %.2f mm: %.1f N.m, %.0f N at the cuff" % (wear, torque, torque / CUFF_RADIUS_M))
    print("  All clamping is lost by %.2f mm of wear, or %.1f mm if the nuts follow it."
          % (travel, WEAR_GAP_MM))


if __name__ == "__main__":
    main()
