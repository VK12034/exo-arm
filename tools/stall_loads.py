"""First-pass stall loads on the elbow drivetrain and hard stops.

Run with: python tools/stall_loads.py

Geometry comes from cad/BudgetExoArm.design.json. Material and component
limits are typical published values, not figures for the parts you will buy,
and worm efficiency comes from a catalogue rather than a measurement. This sizes
the problem. It is not a structural sign-off.
"""

import math

# --- Drivetrain ---
MOTOR_HOLDING_NM = 1.9
WORM_RATIO = 15.0
CHAIN_RATIO = 2.0
CHAIN_EFFICIENCY = 0.97
# Motovario catalogue, NMRV 030 at i=15: static 0.55, dynamic 0.78 at 1400 rpm.
# Other makers may differ, and it will be lower at the speeds used here.
WORM_EFFICIENCIES = (0.55, 0.78)

# --- Geometry, metres ---
PITCH_RADIUS_30T = 0.00953 / (2.0 * math.sin(math.pi / 30.0))
CUFF_RADIUS = 0.193
STOP_RADIUS = 0.03005
STOP_CONTACT_OFFSET = 0.0177
STOP_LEVER = math.sqrt(STOP_RADIUS**2 - STOP_CONTACT_OFFSET**2)
STOP_LOAD_HEIGHT = 0.0034  # strut face to the middle of the contact
STRUT_THICKNESS = 0.0048

# --- Static load, 70 kg wearer, forearm horizontal ---
GRAVITY = 9.81
FOREARM_MODULE = (0.4257, 0.0501)  # kg, m from the elbow, from the CAD
WEARER_FOREARM_HAND = (0.022 * 70.0, 0.179)  # segment fractions, not measured

# --- Typical limits ---
GEARBOX_MAX_OUTPUT_NM = 25.0  # distributor listing for Motovario NMRV030 15:1
CHAIN_ALLOWABLE_N = 2135.0  # about 480 lb, published #35 maximum allowable load
CHAIN_MIN_TENSILE_N = 7830.0  # 1760 lb, Tsubaki RS35 minimum
M5_MINOR_DIAMETER = 0.00424
M5_STRESS_AREA = 14.2e-6
BOLT_YIELD_PA = 640e6  # grade 8.8
BOLT_SHEAR_PA = 0.6 * BOLT_YIELD_PA
ALU_BEARING_YIELD_PA = 386e6  # 6061-T6


def static_load_nm():
    return sum(mass * GRAVITY * arm for mass, arm in (FOREARM_MODULE, WEARER_FOREARM_HAND))


def elbow_torque(worm_efficiency):
    return MOTOR_HOLDING_NM * WORM_RATIO * worm_efficiency * CHAIN_RATIO * CHAIN_EFFICIENCY


def stop_torque_limit():
    """Elbow torque at which the first hard-stop check reaches its limit."""
    section = math.pi * M5_MINOR_DIAMETER**3 / 32.0
    limits = {
        "M5 bending, if the clamp does not help": BOLT_YIELD_PA * section / STOP_LOAD_HEIGHT,
        "M5 shear": BOLT_SHEAR_PA * M5_STRESS_AREA,
        "M5 bearing in the 4.8 mm strut": ALU_BEARING_YIELD_PA * 0.005 * STRUT_THICKNESS,
    }
    return {name: force * STOP_LEVER for name, force in limits.items()}, limits


def main():
    load = static_load_nm()
    print("Static elbow load: %.1f N.m (assumes a 70 kg wearer)" % load)
    print("Stop lever from pivot: %.1f mm" % (STOP_LEVER * 1000.0))
    print()
    print("Worm eff | gearbox out | elbow | x static | at cuff | chain | on a stop")
    for eff in WORM_EFFICIENCIES:
        gearbox = MOTOR_HOLDING_NM * WORM_RATIO * eff
        torque = elbow_torque(eff)
        print(
            "  %.2f   |  %5.1f N.m  | %4.1f  |  %4.1f    | %4.0f N  | %4.0f N | %5.0f N"
            % (
                eff,
                gearbox,
                torque,
                torque / load,
                torque / CUFF_RADIUS,
                gearbox / (0.00953 / (2.0 * math.sin(math.pi / 15.0))),
                torque / STOP_LEVER,
            )
        )
    print()
    print("Limits at the hard stop, as elbow torque:")
    torques, forces = stop_torque_limit()
    for name in torques:
        print("  %-40s %5.0f N  %5.1f N.m" % (name, forces[name], torques[name]))
    weakest = min(torques.values())
    print()
    print("Weakest stop check: %.1f N.m" % weakest)
    for margin in (1.5, 2.0):
        print("With a factor of %.1f: %.1f N.m" % (margin, weakest / margin))
    print("Gearbox maximum output: %.0f N.m, %.1f N.m at the elbow" % (
        GEARBOX_MAX_OUTPUT_NM, GEARBOX_MAX_OUTPUT_NM * CHAIN_RATIO * CHAIN_EFFICIENCY))
    print("Chain allowable %.0f N, minimum tensile %.0f N" % (CHAIN_ALLOWABLE_N, CHAIN_MIN_TENSILE_N))
    print("30T pitch radius %.1f mm" % (PITCH_RADIUS_30T * 1000.0))


if __name__ == "__main__":
    main()
