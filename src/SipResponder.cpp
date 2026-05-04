#include "SipResponder.hpp"

#include <string>

#include "SipStatusCodes.hpp"

#include <pjmedia/sdp.h>
#include <pjsip_ua.h>

#include "utils/log.hpp"

namespace SIPI {

namespace {

void answer_and_send(pjsip_inv_session* inv, int code, const pjmedia_sdp_session* sdp) {
    pjsip_tx_data* tdata = nullptr;
    if (pjsip_inv_answer(inv, code, nullptr, sdp, &tdata) != PJ_SUCCESS || tdata == nullptr) {
        Log::app()->warn("SipResponder: inv_answer({}) failed", code);
        return;
    }
    if (pjsip_inv_send_msg(inv, tdata) != PJ_SUCCESS) {
        Log::app()->warn("SipResponder: send_msg({}) failed", code);
        return;
    }
    Log::app()->debug("SipResponder: sent {}", code);
}

} // namespace

void SipResponder::send_trying(pjsip_inv_session* inv) {
    answer_and_send(inv, kSipTrying, nullptr);
}

void SipResponder::send_ringing(pjsip_inv_session* inv) {
    answer_and_send(inv, kSipRinging, nullptr);
}

void SipResponder::send_ok(pjsip_inv_session* inv, std::string_view sdp_body) {
    std::string          mutable_sdp{sdp_body};
    pjmedia_sdp_session* session = nullptr;
    if (pjmedia_sdp_parse(inv->dlg->pool, mutable_sdp.data(), mutable_sdp.size(), &session) != PJ_SUCCESS) {
        Log::app()->warn("SipResponder: SDP parse failed, sending 500");
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
