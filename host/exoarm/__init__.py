"""Host-side tools for the exoskeleton arm.

Typical use::

    from exoarm import ArmLink, Mode

    with ArmLink("COM5") as link:
        link.set_mode(Mode.HOLD)
        print(link.wait_for_telemetry())
"""

from .link import ArmLink, FrameParser, Heartbeat, LinkStats
from .protocol import Fault, JointState, Mode, MsgId, Telemetry

__all__ = [
    "ArmLink",
    "Fault",
    "FrameParser",
    "Heartbeat",
    "JointState",
    "LinkStats",
    "Mode",
    "MsgId",
    "Telemetry",
]

__version__ = "0.1.0"
