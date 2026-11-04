// Framing, parsing and dispatch for the USB serial link.
#pragma once

#include <Arduino.h>

#include "joint.h"
#include "protocol.h"
#include "safety.h"

class Telemetry {
public:
    void begin(uint32_t baud);

    // The assist gain lives with the control loop, not here, so the link is
    // handed the destination to write when the host changes it.
    void bindAssistGain(volatile float* gain) { assist_gain_ = gain; }

    // Drains the receive buffer and applies any complete, valid frames.
    // Returns true when at least one frame was applied, which the caller
    // uses to feed the host watchdog.
    bool poll(Joint* joints, uint8_t count, Safety* safety, uint32_t now_ms);

    // Emits one telemetry frame. Called at cfg::kTelemetryHz from the main
    // loop rather than from the control ISR.
    void sendTelemetry(const Joint* joints, uint8_t count,
                       const Safety& safety, uint32_t now_ms);

    void sendLog(const char* text);

private:
    void sendFrame(uint8_t msg_id, const void* payload, uint8_t len);
    void handleFrame(uint8_t msg_id, const uint8_t* payload, uint8_t len,
                     Joint* joints, uint8_t count, Safety* safety);
    void sendAck(uint8_t msg_id, bool accepted);

    enum class RxState : uint8_t { kSync0, kSync1, kId, kLen, kPayload, kCrcLo, kCrcHi };

    volatile float* assist_gain_ = nullptr;

    RxState rx_state_ = RxState::kSync0;
    uint8_t rx_id_    = 0;
    uint8_t rx_len_   = 0;
    uint8_t rx_index_ = 0;
    uint8_t rx_buf_[proto::kMaxPayload];
    uint16_t rx_crc_  = 0;
};
