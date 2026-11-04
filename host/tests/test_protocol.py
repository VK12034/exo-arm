"""Round-trip and wire-format tests for the serial protocol."""

from __future__ import annotations

import struct

import pytest

from exoarm import protocol as p
from exoarm.link import FrameParser


def test_crc16_known_vector():
    # CRC-16/CCITT-FALSE check value for "123456789".
    assert p.crc16(b"123456789") == 0x29B1


def test_frame_layout():
    frame = p.encode_frame(p.MsgId.HEARTBEAT)

    assert frame[0] == p.SYNC0
    assert frame[1] == p.SYNC1
    assert frame[2] == p.MsgId.HEARTBEAT
    assert frame[3] == 0
    assert len(frame) == 6

    crc = int.from_bytes(frame[4:], "little")
    assert crc == p.crc16(frame[2:4])


def test_rejects_oversized_payload():
    with pytest.raises(ValueError):
        p.encode_frame(p.MsgId.LOG, b"x" * (p.MAX_PAYLOAD + 1))


@pytest.mark.parametrize("joint", [-1, p.JOINT_COUNT, 99])
def test_rejects_out_of_range_joint(joint):
    with pytest.raises(ValueError):
        p.set_target(joint, 0.0)


def test_set_gains_payload_round_trips():
    frame = p.set_gains(1, 9.0, 1.5, 0.25)
    payload = frame[4:-2]

    joint, kp, ki, kd = struct.unpack("<B3f", payload)
    assert joint == 1
    assert kp == pytest.approx(9.0)
    assert ki == pytest.approx(1.5)
    assert kd == pytest.approx(0.25)


def _telemetry_payload(**over) -> bytes:
    values = dict(t_ms=1234, mode=int(p.Mode.ASSIST), faults=int(p.Fault.LIMIT), deadman=1)
    values.update(over)

    joints = []
    for i in range(p.JOINT_COUNT):
        joints += [0.1 * (i + 1), 0.2, 0.3, 1.5, -0.4]

    return struct.pack(
        "<IBBBB" + "5f" * p.JOINT_COUNT,
        values["t_ms"],
        values["mode"],
        values["faults"],
        values["deadman"],
        p.PROTOCOL_VERSION,
        *joints,
    )


def test_decode_telemetry():
    frame = p.decode_telemetry(_telemetry_payload())

    assert frame.t_ms == 1234
    assert frame.mode is p.Mode.ASSIST
    assert frame.faults is p.Fault.LIMIT
    assert frame.faulted
    assert frame.deadman is True
    assert frame.version == p.PROTOCOL_VERSION
    assert len(frame.joints) == p.JOINT_COUNT
    assert frame.joints[0].position == pytest.approx(0.1)
    assert frame.joints[1].position == pytest.approx(0.2)
    assert frame.joints[0].duty == pytest.approx(-0.4)


def test_telemetry_payload_size_matches_firmware():
    assert len(_telemetry_payload()) == 48


def test_no_faults_reads_as_not_faulted():
    frame = p.decode_telemetry(_telemetry_payload(faults=0))
    assert frame.faults is p.Fault.NONE
    assert not frame.faulted


class TestFrameParser:
    def test_parses_a_single_frame(self):
        parser = FrameParser()
        frames = list(parser.feed(p.encode_frame(p.MsgId.HEARTBEAT)))

        assert frames == [(int(p.MsgId.HEARTBEAT), b"")]
        assert parser.dropped == 0

    def test_parses_frames_split_across_reads(self):
        parser = FrameParser()
        wire = p.set_target(0, 1.25)

        assert list(parser.feed(wire[:3])) == []
        assert list(parser.feed(wire[3:5])) == []
        frames = list(parser.feed(wire[5:]))

        assert len(frames) == 1
        msg_id, payload = frames[0]
        assert msg_id == int(p.MsgId.SET_TARGET)
        assert struct.unpack("<Bf", payload) == (0, pytest.approx(1.25))

    def test_skips_leading_garbage(self):
        parser = FrameParser()
        frames = list(parser.feed(b"\x00\x11garbage" + p.heartbeat()))

        assert len(frames) == 1
        assert parser.dropped == 9

    def test_drops_frame_with_bad_crc(self):
        parser = FrameParser()
        wire = bytearray(p.set_target(1, 0.5))
        wire[-1] ^= 0xFF

        assert list(parser.feed(bytes(wire))) == []
        assert parser.dropped > 0

    def test_recovers_after_a_corrupt_frame(self):
        parser = FrameParser()
        corrupt = bytearray(p.set_target(1, 0.5))
        corrupt[-1] ^= 0xFF

        frames = list(parser.feed(bytes(corrupt) + p.heartbeat()))

        assert len(frames) == 1
        assert frames[0][0] == int(p.MsgId.HEARTBEAT)

    def test_handles_several_frames_in_one_read(self):
        parser = FrameParser()
        wire = p.heartbeat() + p.set_mode(p.Mode.HOLD) + p.clear_fault()

        frames = list(parser.feed(wire))

        assert [f[0] for f in frames] == [
            int(p.MsgId.HEARTBEAT),
            int(p.MsgId.SET_MODE),
            int(p.MsgId.CLEAR_FAULT),
        ]

    def test_ignores_implausible_length_byte(self):
        parser = FrameParser()
        bogus = bytes((p.SYNC0, p.SYNC1, p.MsgId.LOG, p.MAX_PAYLOAD + 10))

        frames = list(parser.feed(bogus + p.heartbeat()))

        assert len(frames) == 1
        assert frames[0][0] == int(p.MsgId.HEARTBEAT)
