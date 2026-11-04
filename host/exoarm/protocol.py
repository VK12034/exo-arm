"""Wire format for the serial link to the arm.

This mirrors ``firmware/include/protocol.h`` byte for byte. Change the two
together, and bump :data:`PROTOCOL_VERSION` whenever the layout changes.

A frame is::

    0xA5 0x5A | msg_id (u8) | len (u8) | payload[len] | crc16 (u16, LE)

The CRC is CRC-16/CCITT-FALSE over msg_id, len and payload.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass
from enum import IntEnum, IntFlag

SYNC0 = 0xA5
SYNC1 = 0x5A

PROTOCOL_VERSION = 1
MAX_PAYLOAD = 64

JOINT_COUNT = 2
JOINT_NAMES = ("shoulder", "elbow")


class MsgId(IntEnum):
    """Message identifiers, in both directions."""

    TELEMETRY = 0x01
    LOG = 0x02
    ACK = 0x03

    SET_MODE = 0x10
    SET_TARGET = 0x11
    SET_GAINS = 0x12
    ESTOP = 0x13
    CLEAR_FAULT = 0x14
    SET_ASSIST = 0x15
    HEARTBEAT = 0x16


class Mode(IntEnum):
    IDLE = 0
    HOLD = 1
    POSITION = 2
    ASSIST = 3


class Fault(IntFlag):
    NONE = 0
    ESTOP = 1 << 0
    OVER_CURRENT = 1 << 1
    DRIVER = 1 << 2
    LIMIT = 1 << 3
    HOST_TIMEOUT = 1 << 4
    ENCODER = 1 << 5


# '<' keeps struct at native sizes with no padding, matching #pragma pack(1).
_TELEMETRY = struct.Struct("<IBBBB" + "5f" * JOINT_COUNT)
_SET_MODE = struct.Struct("<B")
_SET_TARGET = struct.Struct("<Bf")
_SET_GAINS = struct.Struct("<B3f")
_SET_ASSIST = struct.Struct("<f")
_ACK = struct.Struct("<BB")

assert _TELEMETRY.size == 48, "Telemetry layout drifted from protocol.h"
assert _SET_GAINS.size == 13, "SetGainsCmd layout drifted from protocol.h"


@dataclass(frozen=True)
class JointState:
    position: float
    velocity: float
    target: float
    current: float
    duty: float


@dataclass(frozen=True)
class Telemetry:
    t_ms: int
    mode: Mode
    faults: Fault
    deadman: bool
    version: int
    joints: tuple[JointState, ...]

    @property
    def faulted(self) -> bool:
        return self.faults != Fault.NONE


@dataclass(frozen=True)
class Ack:
    msg_id: int
    accepted: bool


def crc16(data: bytes, crc: int = 0xFFFF) -> int:
    """CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection."""
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def encode_frame(msg_id: int, payload: bytes = b"") -> bytes:
    """Wraps a payload in sync bytes, header and CRC."""
    if len(payload) > MAX_PAYLOAD:
        raise ValueError(f"payload of {len(payload)} exceeds {MAX_PAYLOAD} bytes")

    header = bytes((msg_id, len(payload)))
    crc = crc16(header + payload)
    return bytes((SYNC0, SYNC1)) + header + payload + struct.pack("<H", crc)


def decode_telemetry(payload: bytes) -> Telemetry:
    fields = _TELEMETRY.unpack(payload)
    t_ms, mode, faults, deadman, version = fields[:5]

    joints = tuple(
        JointState(*fields[5 + i * 5 : 10 + i * 5]) for i in range(JOINT_COUNT)
    )
    return Telemetry(
        t_ms=t_ms,
        mode=Mode(mode),
        faults=Fault(faults),
        deadman=bool(deadman),
        version=version,
        joints=joints,
    )


def decode_ack(payload: bytes) -> Ack:
    msg_id, accepted = _ACK.unpack(payload)
    return Ack(msg_id=msg_id, accepted=bool(accepted))


def set_mode(mode: Mode) -> bytes:
    return encode_frame(MsgId.SET_MODE, _SET_MODE.pack(int(mode)))


def set_target(joint: int, radians: float) -> bytes:
    _check_joint(joint)
    return encode_frame(MsgId.SET_TARGET, _SET_TARGET.pack(joint, radians))


def set_gains(joint: int, kp: float, ki: float, kd: float) -> bytes:
    _check_joint(joint)
    return encode_frame(MsgId.SET_GAINS, _SET_GAINS.pack(joint, kp, ki, kd))


def set_assist(gain: float) -> bytes:
    return encode_frame(MsgId.SET_ASSIST, _SET_ASSIST.pack(gain))


def estop() -> bytes:
    return encode_frame(MsgId.ESTOP)


def clear_fault() -> bytes:
    return encode_frame(MsgId.CLEAR_FAULT)


def heartbeat() -> bytes:
    return encode_frame(MsgId.HEARTBEAT)


def _check_joint(joint: int) -> None:
    if not 0 <= joint < JOINT_COUNT:
        raise ValueError(f"joint must be 0..{JOINT_COUNT - 1}, got {joint}")
