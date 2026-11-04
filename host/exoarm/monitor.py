"""Live plot of joint angles, currents and duty cycle."""

from __future__ import annotations

import math

from .link import ArmLink
from .protocol import JOINT_NAMES, Fault


def run(link: ArmLink, window_s: float = 10.0, refresh_hz: float = 20.0) -> None:
    """Opens a matplotlib window and streams telemetry into it until closed."""
    import matplotlib.pyplot as plt
    from matplotlib.animation import FuncAnimation

    fig, (ax_pos, ax_cur, ax_duty) = plt.subplots(3, 1, sharex=True, figsize=(9, 7))
    fig.canvas.manager.set_window_title(f"exo-arm · {link.port}")

    pos_lines, target_lines, cur_lines, duty_lines = [], [], [], []
    for name in JOINT_NAMES:
        pos_lines.append(ax_pos.plot([], [], label=f"{name} angle")[0])
        target_lines.append(ax_pos.plot([], [], "--", alpha=0.5, label=f"{name} target")[0])
        cur_lines.append(ax_cur.plot([], [], label=name)[0])
        duty_lines.append(ax_duty.plot([], [], label=name)[0])

    ax_pos.set_ylabel("angle (deg)")
    ax_cur.set_ylabel("current (A)")
    ax_duty.set_ylabel("duty")
    ax_duty.set_xlabel("time (s)")
    ax_duty.set_ylim(-1.05, 1.05)

    for ax in (ax_pos, ax_cur, ax_duty):
        ax.grid(alpha=0.3)
        ax.legend(loc="upper left", fontsize="small")

    status = fig.text(0.01, 0.97, "", fontsize="small", va="top", family="monospace")

    def update(_frame: int):
        history = link.history()
        if not history:
            return []

        t0 = history[-1].t_ms / 1000.0
        times = [f.t_ms / 1000.0 - t0 for f in history]

        visible = [i for i, t in enumerate(times) if t >= -window_s]
        if not visible:
            return []
        lo = visible[0]
        times = times[lo:]
        frames = history[lo:]

        for j in range(len(JOINT_NAMES)):
            pos_lines[j].set_data(times, [math.degrees(f.joints[j].position) for f in frames])
            target_lines[j].set_data(times, [math.degrees(f.joints[j].target) for f in frames])
            cur_lines[j].set_data(times, [f.joints[j].current for f in frames])
            duty_lines[j].set_data(times, [f.joints[j].duty for f in frames])

        for ax in (ax_pos, ax_cur, ax_duty):
            ax.set_xlim(-window_s, 0.5)
        for ax in (ax_pos, ax_cur):
            ax.relim()
            ax.autoscale_view(scalex=False)

        status.set_text(_status_line(link))
        return pos_lines + target_lines + cur_lines + duty_lines

    # cache_frame_data=False keeps FuncAnimation from retaining every frame.
    _anim = FuncAnimation(
        fig, update, interval=int(1000 / refresh_hz), blit=False, cache_frame_data=False
    )
    plt.tight_layout()
    plt.show()


def _status_line(link: ArmLink) -> str:
    frame = link.latest()
    if frame is None:
        return "waiting for telemetry"

    faults = "none" if frame.faults == Fault.NONE else str(frame.faults).replace("Fault.", "")
    return (
        f"mode={frame.mode.name:<8} faults={faults:<24} "
        f"deadman={'held' if frame.deadman else 'open':<5} "
        f"frames={link.stats.frames} dropped={link.stats.dropped_bytes}"
    )


def format_status(link: ArmLink) -> str | None:
    """One-line summary used by the ``status`` command."""
    frame = link.latest()
    if frame is None:
        return None

    parts = [_status_line(link)]
    for name, joint in zip(JOINT_NAMES, frame.joints, strict=True):
        parts.append(
            f"  {name:<9} {math.degrees(joint.position):7.2f} deg  "
            f"target {math.degrees(joint.target):7.2f} deg  "
            f"{joint.velocity:6.3f} rad/s  {joint.current:5.2f} A  duty {joint.duty:+.3f}"
        )
    return "\n".join(parts)
