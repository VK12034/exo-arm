"""First-pass stall loads on the elbow drivetrain and hard stops.

Run with: python tools/stall_loads.py

Geometry comes from cad/BudgetExoArm.design.json. Material and component
limits are typical published values, not figures for the parts you will buy,
and worm efficiency comes from a catalogue rather than a measurement. This sizes
the problem. It is not a structural sign-off.
"""

import math

# --- Drivetrain ---
MOTOR_HOLDING_NM = 0.55  # 23HS16-2804 listing, at its rated current
MOTOR_RATED_A = 2.8
DRIVER_START_A = 2.0  # first setting to try, see docs/commissioning.md
DRIVER_MAX_A = 3.5  # highest setting on a typical TB6600 table
WORM_RATIO = 30.0
CHAIN_RATIO = 2.0
CHAIN_EFFICIENCY = 0.97
# Motovario catalogue, NMRV 030 at i=30: static 0.39, dynamic 0.66 at 1400 rpm.
# Other makers may differ. Running efficiency here lies between the two.
WORM_EFFICIENCIES = (0.39, 0.66)

# --- Geometry, metres ---
PITCH_RADIUS_15T = 0.00953 / (2.0 * math.sin(math.pi / 15.0))
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
# Motovario catalogue, NRV 030 at i=30 and 500 rpm input, section 5.12. Section 2
# allows peaks up to twice Mn2.
GEARBOX_MN2_NM = 20.0
GEARBOX_PEAK_NM = 2.0 * GEARBOX_MN2_NM
GEARBOX_FR2_N = 1529.0
CHAIN_WHEEL_FACTOR = 1.4  # catalogue section 2.11, fz for a chain wheel
# Catalogue section 2.11.3, size 030: Frx = Fr2 * a / (b + x), for a load x mm out
# from the shaft shoulder. 50 mm is a pessimistic offset, not a measured one.
FR2_A_MM, FR2_B_MM, FR2_OFFSET_MM = 65.0, 50.0, 50.0
CHAIN_ALLOWABLE_N = 2135.0  # about 480 lb, published #35 maximum allowable load
CHAIN_MIN_TENSILE_N = 7830.0  # 1760 lb, Tsubaki RS35 minimum
M5_MINOR_DIAMETER = 0.00424
M5_STRESS_AREA = 14.2e-6
BOLT_YIELD_PA = 640e6  # grade 8.8
BOLT_SHEAR_PA = 0.6 * BOLT_YIELD_PA
ALU_BEARING_YIELD_PA = 386e6  # 6061-T6

# --- Motor speed check ---
MOTOR_INDUCTANCE_H = 1.4e-3  # listing, +/- 20 %
SUPPLY_V = 24.0
TOP_SPEED_DEG_S = 20.0


def static_load_nm():
    return sum(mass * GRAVITY * arm for mass, arm in (FOREARM_MODULE, WEARER_FOREARM_HAND))


def motor_torque(amps):
    """Holding torque in proportion to current. Generous above rated, where it saturates."""
    return MOTOR_HOLDING_NM * amps / MOTOR_RATED_A


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
    print("Driver A | worm eff | motor | gearbox out | elbow | x static | at cuff | chain | radial | on a stop")
    for amps in (DRIVER_START_A, MOTOR_RATED_A, DRIVER_MAX_A):
        motor = motor_torque(amps)
        for eff in WORM_EFFICIENCIES:
            gearbox = motor * WORM_RATIO * eff
            torque = gearbox * CHAIN_RATIO * CHAIN_EFFICIENCY
            chain = gearbox / PITCH_RADIUS_15T
            print(
                "  %.1f    |   %.2f   | %.2f  |  %5.1f N.m  | %4.1f  |  %4.1f    | %4.0f N  | %4.0f N | %4.0f N | %5.0f N"
                % (
                    amps,
                    eff,
                    motor,
                    gearbox,
                    torque,
                    torque / load,
                    torque / CUFF_RADIUS,
                    chain,
                    chain * CHAIN_WHEEL_FACTOR,
                    torque / STOP_LEVER,
                )
            )
    print()
    needed = load / (CHAIN_RATIO * CHAIN_EFFICIENCY * WORM_RATIO * WORM_EFFICIENCIES[0])
    print("Motor torque needed for the static load at the lower efficiency: %.2f N.m" % needed)
    print("Margin at %.1f A: %.1f times" % (DRIVER_START_A, motor_torque(DRIVER_START_A) / needed))
    rpm = TOP_SPEED_DEG_S / 360.0 * 60.0 * WORM_RATIO * CHAIN_RATIO
    step_ms = 60000.0 / (rpm * 200.0)
    rise_ms = MOTOR_INDUCTANCE_H * MOTOR_RATED_A / SUPPLY_V * 1000.0
    print("Motor at %.0f deg/s: %.0f rpm, %.2f ms per full step, %.2f ms for the phase current to rise"
          % (TOP_SPEED_DEG_S, rpm, step_ms, rise_ms))
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
    print("Gearbox Mn2 %.0f N.m, peak %.0f N.m, %.1f and %.1f N.m at the elbow" % (
        GEARBOX_MN2_NM,
        GEARBOX_PEAK_NM,
        GEARBOX_MN2_NM * CHAIN_RATIO * CHAIN_EFFICIENCY,
        GEARBOX_PEAK_NM * CHAIN_RATIO * CHAIN_EFFICIENCY,
    ))
    print("Radial limit %.0f N on the centre line, %.0f N at %.0f mm out" % (
        GEARBOX_FR2_N, GEARBOX_FR2_N * FR2_A_MM / (FR2_B_MM + FR2_OFFSET_MM), FR2_OFFSET_MM))
    print("Chain allowable %.0f N, minimum tensile %.0f N" % (CHAIN_ALLOWABLE_N, CHAIN_MIN_TENSILE_N))
    print("Pitch radius 15T %.1f mm, 30T %.1f mm" % (PITCH_RADIUS_15T * 1000.0, PITCH_RADIUS_30T * 1000.0))


if __name__ == "__main__":
    main()
