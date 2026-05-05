#pragma once

#include <string_view>

#include <pjsip_ua.h>

namespace SIPI {

struct SipResponder {
    static void send_trying(pjsip_inv_session* inv, pjsip_rx_data* rdata);
    static void send_ringing(pjsip_inv_session* inv);
    static void send_ok(pjsip_inv_session* inv, std::string_view sdp_body);
    static void send_request_terminated(pjsip_inv_session* inv);
    static void send_not_acceptable(pjsip_inv_session* inv);
};

} // namespace SIPI
