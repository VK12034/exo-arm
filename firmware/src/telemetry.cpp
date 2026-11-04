#include "telemetry.h"

#include <string.h>

void Telemetry::begin(uint32_t baud) {
    Serial.begin(baud);
}

void Telemetry::sendFrame(uint8_t msg_id, const void* payload, uint8_t len) {
    uint8_t header[2] = { msg_id, len };

    uint16_t crc = proto::crc16(header, sizeof(header));
    crc = proto::crc16(static_cast<const uint8_t*>(payload), len, crc);

    Serial.write(proto::kSync0);
    Serial.write(proto::kSync1);
    Serial.write(header, sizeof(header));
    if (len > 0) Serial.write(static_cast<const uint8_t*>(payload), len);
    Serial.write(static_cast<uint8_t>(crc & 0xFF));
    Serial.write(static_cast<uint8_t>(crc >> 8));
}

void Telemetry::sendAck(uint8_t msg_id, bool accepted) {
    const uint8_t payload[2] = { msg_id, static_cast<uint8_t>(accepted ? 1 : 0) };
    sendFrame(proto::kMsgAck, payload, sizeof(payload));
}

void Telemetry::sendLog(const char* text) {
    const size_t len = strnlen(text, proto::kMaxPayload);
    sendFrame(proto::kMsgLog, text, static_cast<uint8_t>(len));
}

void Telemetry::sendTelemetry(const Joint* joints, uint8_t count,
                              const Safety& safety, uint32_t now_ms) {
    proto::Telemetry t{};

    // The control loop runs from a timer interrupt and writes the same
    // joint state this reads, so the snapshot is taken with interrupts off.
    // Only the copy is protected; the serial write below is not.
    noInterrupts();

    t.t_ms    = now_ms;
    t.mode    = static_cast<uint8_t>(safety.mode());
    t.faults  = safety.faults();
    t.deadman = safety.deadmanHeld() ? 1 : 0;
    t.version = proto::kProtocolVersion;

    const uint8_t n = count < 2 ? count : 2;
    for (uint8_t i = 0; i < n; ++i) {
        t.joint[i].position = joints[i].position();
        t.joint[i].velocity = joints[i].velocity();
        t.joint[i].target   = joints[i].target();
        t.joint[i].current  = joints[i].current();
        t.joint[i].duty     = joints[i].duty();
    }

    interrupts();

    sendFrame(proto::kMsgTelemetry, &t, sizeof(t));
}

bool Telemetry::poll(Joint* joints, uint8_t count, Safety* safety, uint32_t now_ms) {
    bool applied = false;

    while (Serial.available() > 0) {
        const uint8_t byte = static_cast<uint8_t>(Serial.read());

        switch (rx_state_) {
        case RxState::kSync0:
            if (byte == proto::kSync0) rx_state_ = RxState::kSync1;
            break;

        case RxState::kSync1:
            // A byte that is not the second sync might itself be the start of
            // a real frame, so fall back to looking for sync0 rather than
            // discarding it outright.
            rx_state_ = (byte == proto::kSync1) ? RxState::kId
                      : (byte == proto::kSync0) ? RxState::kSync1
                                                : RxState::kSync0;
            break;

        case RxState::kId:
            rx_id_    = byte;
            rx_state_ = RxState::kLen;
            break;

        case RxState::kLen:
            rx_len_ = byte;
            if (rx_len_ > proto::kMaxPayload) {
                rx_state_ = RxState::kSync0;  // bogus length, resynchronise
                break;
            }
            rx_index_ = 0;
            rx_state_ = (rx_len_ == 0) ? RxState::kCrcLo : RxState::kPayload;
            break;

        case RxState::kPayload:
            rx_buf_[rx_index_++] = byte;
            if (rx_index_ >= rx_len_) rx_state_ = RxState::kCrcLo;
            break;

        case RxState::kCrcLo:
            rx_crc_   = byte;
            rx_state_ = RxState::kCrcHi;
            break;

        case RxState::kCrcHi: {
            rx_crc_ |= static_cast<uint16_t>(byte) << 8;

            const uint8_t header[2] = { rx_id_, rx_len_ };
            uint16_t crc = proto::crc16(header, sizeof(header));
            crc = proto::crc16(rx_buf_, rx_len_, crc);

            if (crc == rx_crc_) {
                handleFrame(rx_id_, rx_buf_, rx_len_, joints, count, safety);
                safety->noteHostMessage(now_ms);
                applied = true;
            }
            rx_state_ = RxState::kSync0;
            break;
        }
        }
    }

    return applied;
}

void Telemetry::handleFrame(uint8_t msg_id, const uint8_t* payload, uint8_t len,
                            Joint* joints, uint8_t count, Safety* safety) {
    switch (msg_id) {
    case proto::kCmdHeartbeat:
        // Receiving it is the whole point; noteHostMessage already ran.
        break;

    case proto::kCmdSetMode: {
        if (len != sizeof(proto::SetModeCmd)) { sendAck(msg_id, false); return; }
        proto::SetModeCmd cmd;
        memcpy(&cmd, payload, sizeof(cmd));
        if (cmd.mode > static_cast<uint8_t>(proto::Mode::kAssist)) {
            sendAck(msg_id, false);
            return;
        }
        safety->requestMode(static_cast<proto::Mode>(cmd.mode));
        sendAck(msg_id, safety->mode() == static_cast<proto::Mode>(cmd.mode));
        return;
    }

    case proto::kCmdSetTarget: {
        if (len != sizeof(proto::SetTargetCmd)) { sendAck(msg_id, false); return; }
        proto::SetTargetCmd cmd;
        memcpy(&cmd, payload, sizeof(cmd));
        if (cmd.joint >= count) { sendAck(msg_id, false); return; }
        joints[cmd.joint].setTarget(cmd.target);
        break;
    }

    case proto::kCmdSetGains: {
        if (len != sizeof(proto::SetGainsCmd)) { sendAck(msg_id, false); return; }
        proto::SetGainsCmd cmd;
        memcpy(&cmd, payload, sizeof(cmd));
        if (cmd.joint >= count) { sendAck(msg_id, false); return; }
        joints[cmd.joint].setGains(cmd.kp, cmd.ki, cmd.kd);
        break;
    }

    case proto::kCmdEStop:
        safety->requestMode(proto::Mode::kIdle);
        for (uint8_t i = 0; i < count; ++i) joints[i].disable();
        break;

    case proto::kCmdClearFault:
        sendAck(msg_id, safety->clearFaults());
        return;

    case proto::kCmdSetAssist: {
        if (len != sizeof(proto::SetAssistCmd)) { sendAck(msg_id, false); return; }
        if (assist_gain_ == nullptr) { sendAck(msg_id, false); return; }
        proto::SetAssistCmd cmd;
        memcpy(&cmd, payload, sizeof(cmd));
        *assist_gain_ = cmd.gain;
        break;
    }

    default:
        sendAck(msg_id, false);
        return;
    }

    sendAck(msg_id, true);
}
