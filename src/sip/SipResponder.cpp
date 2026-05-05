#include "SipResponder.hpp"

#include <string>

#include "SipStatusCodes.hpp"

#include <pjmedia/sdp.h>
#include <pjsip_ua.h>

#include "utils/log.hpp"

namespace SIPI {

namespace {

void answer_and_send(pjsip_inv_session* inv, int code, const pjmedia_sdp_session* sdp) {
    // Use pjsip_inv_answer to construct and send a SIP response; includes SDP if provided.
    pjsip_tx_data* tdata = nullptr;
    if (pjsip_inv_answer(inv, code, nullptr, sdp, &tdata) != PJ_SUCCESS || tdata == nullptr) {
        Log::sip()->warn("failed to answer INVITE with {}", code);
        return;
    }
    if (pjsip_inv_send_msg(inv, tdata) != PJ_SUCCESS) {
        Log::sip()->warn("failed to send response {}", code);
        return;
    }
    Log::sip()->debug("sent SIP {}", code);
}

} // namespace

void SipResponder::send_trying(pjsip_inv_session* inv, pjsip_rx_data* rdata) {
    // Use pjsip_inv_initial_answer for the first response (100 Trying) to an INVITE.
    pjsip_tx_data* tdata = nullptr;
    if (pjsip_inv_initial_answer(inv, rdata, kSipTrying, nullptr, nullptr, &tdata) != PJ_SUCCESS || tdata == nullptr) {
        Log::sip()->warn("failed to answer INVITE with 100");
        return;
    }
    if (pjsip_inv_send_msg(inv, tdata) != PJ_SUCCESS) {
        Log::sip()->warn("failed to send 100 Trying");
        return;
    }
    Log::sip()->debug("sent SIP 100");
}

void SipResponder::send_ringing(pjsip_inv_session* inv) {
    answer_and_send(inv, kSipRinging, nullptr);
}

void SipResponder::send_ok(pjsip_inv_session* inv, std::string_view sdp_body) {
    // Parse local SDP and attach it to the 200 OK response for media negotiation.
    std::string mutable_sdp{sdp_body};
    pjmedia_sdp_session* session = nullptr;
    if (pjmedia_sdp_parse(inv->dlg->pool, mutable_sdp.data(), mutable_sdp.size(), &session) != PJ_SUCCESS) {
        Log::sip()->error("failed to parse local SDP, sending 500");
        answer_and_send(inv, kSipInternalError, nullptr);
        return;
    }
    answer_and_send(inv, kSipOk, session);
}

void SipResponder::send_request_terminated(pjsip_inv_session* inv) {
    answer_and_send(inv, kSipRequestTerminated, nullptr);
}

void SipResponder::send_not_acceptable(pjsip_inv_session* inv) {
    answer_and_send(inv, kSipNotAcceptableHere, nullptr);
}

} // namespace SIPI
