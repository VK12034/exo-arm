"""Serial link to the arm: framing, a background reader and command helpers."""

from __future__ import annotations

import threading
import time
from collections import deque
from collections.abc import Callable, Iterator
from dataclasses import dataclass

from . import protocol as p


class FrameParser:
    """Incremental frame parser.

    Feed it whatever bytes arrive; it yields complete, CRC-checked frames and
    silently drops anything malformed, resynchronising on the next sync pair.
    """

    def __init__(self) -> None:
        self._buf = bytearray()
        self.dropped = 0

    def feed(self, data: bytes) -> Iterator[tuple[int, bytes]]:
        self._buf.extend(data)

        while True:
            start = self._buf.find(bytes((p.SYNC0, p.SYNC1)))
            if start < 0:
                # Keep one byte in case a sync pair straddles two reads.
                if len(self._buf) > 1:
                    self.dropped += len(self._buf) - 1
                    del self._buf[:-1]
                return

            if start > 0:
                self.dropped += start
                del self._buf[:start]

            if len(self._buf) < 4:
                return

            msg_id = self._buf[2]
            length = self._buf[3]

            if length > p.MAX_PAYLOAD:
                self.dropped += 1
                del self._buf[:1]
                continue

            total = 4 + length + 2
            if len(self._buf) < total:
                return

            payload = bytes(self._buf[4 : 4 + length])
            crc_rx = int.from_bytes(self._buf[4 + length : total], "little")
            crc_calc = p.crc16(bytes((msg_id, length)) + payload)

            if crc_rx == crc_calc:
                del self._buf[:total]
                yield msg_id, payload
            else:
                # Bad CRC: step past this sync pair and look for the next.
                self.dropped += 1
                del self._buf[:1]


@dataclass
class LinkStats:
    frames: int = 0
    acks: int = 0
    logs: int = 0
    dropped_bytes: int = 0


class ArmLink:
    """Owns the serial port and a reader thread.

    Use as a context manager::

        with ArmLink("COM5") as link:
            link.set_mode(protocol.Mode.HOLD)
            print(link.latest())
    """

    def __init__(
        self,
        port: str,
        baud: int = 921_600,
        history: int = 2000,
        on_log: Callable[[str], None] | None = None,
    ) -> None:
        self.port = port
        self.baud = baud
        self.stats = LinkStats()

        self._serial = None
        self._parser = FrameParser()
        self._thread: threading.Thread | None = None
        self._stop = threading.Event()
        self._lock = threading.Lock()

        self._history: deque[p.Telemetry] = deque(maxlen=history)
        self._latest: p.Telemetry | None = None
        self._last_ack: p.Ack | None = None
        self._on_log = on_log

    # --- lifecycle --------------------------------------------------------

    def open(self) -> ArmLink:
        import serial  # imported here so the module is usable without pyserial

        self._serial = serial.Serial(self.port, self.baud, timeout=0.05)
        self._stop.clear()
        self._thread = threading.Thread(target=self._read_loop, daemon=True)
        self._thread.start()
        return self

    def close(self) -> None:
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=1.0)
            self._thread = None
        if self._serial is not None:
            # Leaving the port open with the arm enabled would be the worst
            # possible exit, so stop it on the way out.
            try:
                self._write(p.estop())
            except Exception:
                pass
            self._serial.close()
            self._serial = None

    def __enter__(self) -> ArmLink:
        return self.open()

    def __exit__(self, *exc_info: object) -> None:
        self.close()

    # --- reading ----------------------------------------------------------

    def _read_loop(self) -> None:
        while not self._stop.is_set():
            try:
                data = self._serial.read(4096)
            except Exception:
                break

            if not data:
                continue

            for msg_id, payload in self._parser.feed(data):
                self._dispatch(msg_id, payload)

            with self._lock:
                self.stats.dropped_bytes = self._parser.dropped

    def _dispatch(self, msg_id: int, payload: bytes) -> None:
        if msg_id == p.MsgId.TELEMETRY:
            frame = p.decode_telemetry(payload)
            with self._lock:
                self._latest = frame
                self._history.append(frame)
                self.stats.frames += 1
        elif msg_id == p.MsgId.ACK:
            with self._lock:
                self._last_ack = p.decode_ack(payload)
                self.stats.acks += 1
        elif msg_id == p.MsgId.LOG:
            with self._lock:
                self.stats.logs += 1
            if self._on_log is not None:
                self._on_log(payload.decode("utf-8", errors="replace"))

    def latest(self) -> p.Telemetry | None:
        with self._lock:
            return self._latest

    def history(self) -> list[p.Telemetry]:
        with self._lock:
            return list(self._history)

    def wait_for_telemetry(self, timeout: float = 2.0) -> p.Telemetry:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            frame = self.latest()
            if frame is not None:
                return frame
            time.sleep(0.01)
        raise TimeoutError(f"no telemetry from {self.port} within {timeout:.1f}s")

    # --- writing ----------------------------------------------------------

    def _write(self, frame: bytes) -> None:
        if self._serial is None:
            raise RuntimeError("link is not open")
        self._serial.write(frame)

    def set_mode(self, mode: p.Mode) -> None:
        self._write(p.set_mode(mode))

    def set_target(self, joint: int, radians: float) -> None:
        self._write(p.set_target(joint, radians))

    def set_gains(self, joint: int, kp: float, ki: float, kd: float) -> None:
        self._write(p.set_gains(joint, kp, ki, kd))

    def set_assist(self, gain: float) -> None:
        self._write(p.set_assist(gain))

    def estop(self) -> None:
        self._write(p.estop())

    def clear_fault(self) -> None:
        self._write(p.clear_fault())

    def heartbeat(self) -> None:
        self._write(p.heartbeat())

    def last_ack(self) -> p.Ack | None:
        with self._lock:
            return self._last_ack


class Heartbeat:
    """Keeps the firmware's host watchdog fed for as long as it is running."""

    def __init__(self, link: ArmLink, period: float = 0.1) -> None:
        self._link = link
        self._period = period
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None

    def __enter__(self) -> Heartbeat:
        self._stop.clear()
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()
        return self

    def __exit__(self, *exc_info: object) -> None:
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=1.0)

    def _run(self) -> None:
        while not self._stop.wait(self._period):
            try:
                self._link.heartbeat()
            except Exception:
                return
