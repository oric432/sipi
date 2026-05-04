#pragma once

#include <cstdint>
#include <string>

#include <pjsip_ua.h>

struct InviteReceived {
    pjsip_inv_session* inv_;
    pjsip_rx_data*     rdata_;
};

struct SdpParsed {
    std::string remote_ip_;
    uint16_t    remote_port_{};
    bool        has_audio_{};
    bool        supports_pcma_{};
};

struct SdpRejected {};
struct RtpReady {};
struct AckReceived {};
struct ByeReceived {};
struct CancelReceived {};
struct TransportError {};
struct TimeoutExpired {};
