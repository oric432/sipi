#pragma once

#include <cstdint>
#include <string>

#include <pjsip_ua.h>

// First event: raw INVITE before PJSIP dialog/inv session creation
struct IncomingInvite {
    pjsip_rx_data* rdata_;
    pjsip_endpoint* endpoint_;
    int mod_id_;
};

// Internal events posted by state machine actions
struct SetupOk {}; // Dialog + inv session created successfully
struct SetupFailed {}; // INVITE verification or dialog/inv creation failed

struct SdpParsed {
    std::string remote_ip_;
    uint16_t remote_port_{};
    bool has_audio_{};
    bool supports_pcma_{};
};

struct SdpRejected {};
struct RtpReady {};
struct AckReceived {};
struct ByeReceived {};
struct CallDisconnected {};
struct CancelReceived {};
struct TransportError {};
struct TimeoutExpired {};
