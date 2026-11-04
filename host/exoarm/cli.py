"""Command line entry point: ``python -m exoarm`` or ``exoarm``."""

from __future__ import annotations

import argparse
import csv
import math
import sys
import time
from collections.abc import Sequence

from .link import ArmLink, Heartbeat
from .protocol import JOINT_COUNT, JOINT_NAMES, Mode


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="exoarm",
        description="Talk to the exoskeleton arm over USB serial.",
    )
    parser.add_argument("--port", required=True, help="serial port, e.g. COM5 or /dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=921_600)

    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("status", help="print one telemetry frame and exit")

    p_monitor = sub.add_parser("monitor", help="live plot of angles, current and duty")
    p_monitor.add_argument("--window", type=float, default=10.0, help="seconds of history")

    p_mode = sub.add_parser("mode", help="change the arm's mode")
    p_mode.add_argument("mode", choices=[m.name.lower() for m in Mode])

    p_move = sub.add_parser("move", help="send a position target")
    p_move.add_argument("--joint", type=int, required=True, choices=range(JOINT_COUNT))
    p_move.add_argument("--deg", type=float, required=True)

    p_gains = sub.add_parser("gains", help="set PID gains for one joint")
    p_gains.add_argument("--joint", type=int, required=True, choices=range(JOINT_COUNT))
    p_gains.add_argument("--kp", type=float, required=True)
    p_gains.add_argument("--ki", type=float, required=True)
    p_gains.add_argument("--kd", type=float, required=True)

    p_assist = sub.add_parser("assist", help="set the admittance gain, (rad/s) per newton")
    p_assist.add_argument("gain", type=float)

    p_record = sub.add_parser("record", help="log telemetry to CSV")
    p_record.add_argument("--seconds", type=float, default=10.0)
    p_record.add_argument("--out", default="telemetry.csv")

    sub.add_parser("estop", help="stop the arm and drop to idle")
    sub.add_parser("clear", help="clear latched faults")

    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)

    try:
        with ArmLink(args.port, args.baud, on_log=lambda m: print(f"[arm] {m}")) as link:
            return _dispatch(args, link)
    except TimeoutError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    except KeyboardInterrupt:
        return 130


def _dispatch(args: argparse.Namespace, link: ArmLink) -> int:
    from . import monitor

    if args.command == "status":
        link.wait_for_telemetry()
        print(monitor.format_status(link))
        return 0

    if args.command == "monitor":
        link.wait_for_telemetry()
        with Heartbeat(link):
            monitor.run(link, window_s=args.window)
        return 0

    if args.command == "mode":
        link.wait_for_telemetry()
        link.set_mode(Mode[args.mode.upper()])
        return _report_ack(link)

    if args.command == "move":
        link.wait_for_telemetry()
        link.set_target(args.joint, math.radians(args.deg))
        return _report_ack(link)

    if args.command == "gains":
        link.wait_for_telemetry()
        link.set_gains(args.joint, args.kp, args.ki, args.kd)
        return _report_ack(link)

    if args.command == "assist":
        link.wait_for_telemetry()
        link.set_assist(args.gain)
        return _report_ack(link)

    if args.command == "estop":
        link.estop()
        time.sleep(0.1)
        print("estop sent")
        return 0

    if args.command == "clear":
        link.wait_for_telemetry()
        link.clear_fault()
        time.sleep(0.1)
        ack = link.last_ack()
        if ack is not None and not ack.accepted:
            print("refused: the fault condition is still present", file=sys.stderr)
            return 1
        print("faults cleared")
        return 0

    if args.command == "record":
        return _record(link, args.seconds, args.out)

    raise AssertionError(f"unhandled command {args.command!r}")


def _report_ack(link: ArmLink, timeout: float = 0.5) -> int:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        ack = link.last_ack()
        if ack is not None:
            if ack.accepted:
                print("ok")
                return 0
            print(f"rejected by the arm (msg 0x{ack.msg_id:02x})", file=sys.stderr)
            return 1
        time.sleep(0.01)

    print("no acknowledgement from the arm", file=sys.stderr)
    return 2


def _record(link: ArmLink, seconds: float, path: str) -> int:
    link.wait_for_telemetry()

    columns = ["t_ms", "mode", "faults", "deadman"]
    for name in JOINT_NAMES:
        columns += [f"{name}_{field}" for field in ("pos", "vel", "target", "current", "duty")]

    print(f"recording {seconds:.1f}s to {path}")
    with Heartbeat(link):
        time.sleep(seconds)

    frames = link.history()
    with open(path, "w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(columns)
        for frame in frames:
            row = [frame.t_ms, frame.mode.name, int(frame.faults), int(frame.deadman)]
            for joint in frame.joints:
                row += [joint.position, joint.velocity, joint.target, joint.current, joint.duty]
            writer.writerow(row)

    print(f"wrote {len(frames)} frames")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
