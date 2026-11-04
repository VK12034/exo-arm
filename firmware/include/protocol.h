// Wire format for the USB serial link between the arm and the host tools.
//
// Every frame is:
//
//   0xA5 0x5A | msg_id (u8) | len (u8) | payload[len] | crc16 (u16, LE)
//
// The CRC is CRC-16/CCITT-FALSE taken over msg_id, len and payload. All
// multi-byte fields are little-endian; both the Teensy and the host are
// little-endian, so the packed structs below map directly onto the wire.
//
// host/exoarm/protocol.py mirrors this file. Change the two together, and
// bump kProtocolVersion whenever the layout changes.
#pragma once

#include <stdint.h>

namespace proto {

constexpr uint8_t kSync0 = 0xA5;
constexpr uint8_t kSync1 = 0x5A;

constexpr uint8_t kProtocolVersion = 1;
constexpr uint8_t kMaxPayload      = 64;

// Device to host.
enum MsgId : uint8_t {
    kMsgTelemetry = 0x01,
    kMsgLog       = 0x02,
    kMsgAck       = 0x03,

    // Host to device.
    kCmdSetMode    = 0x10,
    kCmdSetTarget  = 0x11,
    kCmdSetGains   = 0x12,
    kCmdEStop      = 0x13,
    kCmdClearFault = 0x14,
    kCmdSetAssist  = 0x15,
    kCmdHeartbeat  = 0x16,
};

enum class Mode : uint8_t {
    kIdle     = 0,  // drivers asleep, no output
    kHold     = 1,  // hold the pose the arm was in when the mode was entered
    kPosition = 2,  // follow targets sent by the host
    kAssist   = 3,  // admittance control from the interaction force sensors
};

// Fault bits. Any non-zero value latches the arm into the fault state and
// puts the drivers to sleep; the host must clear it explicitly.
enum FaultFlag : uint8_t {
    kFaultNone        = 0,
    kFaultEStop       = 1 << 0,
    kFaultOverCurrent = 1 << 1,
    kFaultDriver      = 1 << 2,
    kFaultLimit       = 1 << 3,
    kFaultHostTimeout = 1 << 4,
    kFaultEncoder     = 1 << 5,
};

#pragma pack(push, 1)

struct JointTelemetry {
    float position;   // rad
    float velocity;   // rad/s
    float target;     // rad
    float current;    // A
    float duty;       // -1..1 commanded to the driver
};

struct Telemetry {
    uint32_t       t_ms;
    uint8_t        mode;
    uint8_t        faults;
    uint8_t        deadman;    // 1 while the operator grip is held
    uint8_t        version;
    JointTelemetry joint[2];
};

struct SetModeCmd {
    uint8_t mode;
};

struct SetTargetCmd {
    uint8_t joint;
    float   target;   // rad
};

struct SetGainsCmd {
    uint8_t joint;
    float   kp;
    float   ki;
    float   kd;
};

struct SetAssistCmd {
    float gain;       // (rad/s) per newton
};

#pragma pack(pop)

static_assert(sizeof(JointTelemetry) == 20, "JointTelemetry layout changed");
static_assert(sizeof(Telemetry) == 48, "Telemetry layout changed");
static_assert(sizeof(SetGainsCmd) == 13, "SetGainsCmd layout changed");

// CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, no final xor.
inline uint16_t crc16(const uint8_t* data, uint16_t len, uint16_t crc = 0xFFFF) {
    for (uint16_t i = 0; i < len; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                                 : static_cast<uint16_t>(crc << 1);
        }
    }
    return crc;
}

}  // namespace proto
