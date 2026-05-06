#include "SipRouter.hpp"

#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_dialog.h>
#include <pjsip/sip_util.h>
#include <pjsip/sip_ua_layer.h>

#include "CallManager.hpp"
#include "Events.hpp"
#include "SipStatusCodes.hpp"
#include "utils/log.hpp"

namespace SIPI {

SipRouter::SipRouter(CallManager& manager)
    : manager_(manager) {}

void SipRouter::set_endpoint(pjsip_endpoint* endpt) {
    endpt_ = endpt;
}

pj_bool_t SipRouter::on_request(pjsip_rx_data* rdata, int mod_id) {
    if (endpt_ == nullptr) {
        Log::sip()->warn("on_request called but endpoint not ready");
        return PJ_FALSE;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
    const pjsip_method& method = rdata->msg_info.msg->line.req.method;

    // Reject non-INVITE requests with 500
    if (method.id != PJSIP_INVITE_METHOD) {
        if (method.id != PJSIP_ACK_METHOD) {
            auto reason_text = std::to_array("SIP UA unable to handle this request");
            pj_str_t reason = {.ptr = reason_text.data(), .slen = static_cast<pj_ssize_t>(reason_text.size() - 1)};
            pjsip_endpt_respond_stateless(endpt_, rdata, kSipInternalError, &reason, nullptr, nullptr);
        }
        return PJ_TRUE;
    }

    // In-dialog requests (To tag present) are handled by the inv/dialog layer
    if (rdata->msg_info.to != nullptr && rdata->msg_info.to->tag.slen > 0) {
        return PJ_FALSE;
    }

    const std::string call_id(rdata->msg_info.cid->id.ptr, static_cast<std::size_t>(rdata->msg_info.cid->id.slen));
    Log::sip()->debug("[{}] INVITE received", call_id);

    // All PJSIP setup (verify, dialog, inv) happens inside CallContext via the SM.
    // SipRouter is now a thin classifier: method filter + in-dialog check.
    manager_.on_incoming_invite(rdata, endpt_, mod_id);
    return PJ_TRUE;
}

void SipRouter::on_inv_state_changed(pjsip_inv_session* inv, int mod_id) {
    if (inv == nullptr || mod_id < 0) {
        return;
    }

    if (inv->state == PJSIP_INV_STATE_CONFIRMED) {
        Log::sip()->debug("INVITE confirmed (ACK received)");
        manager_.dispatch(inv, mod_id, AckReceived{});
        return;
    }

    if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {
        if (inv->cancelling != 0) {
            Log::sip()->info("call cancelled");
            manager_.dispatch(inv, mod_id, CancelReceived{});
        }
        else {
            // Translate generic PJSIP DISCONNECTED (could be BYE, timeout, etc.) to
            // CallDisconnected event. CallManager will route to session, dispatch event,
            // and cleanup mod_data/session lifecycle.
            Log::sip()->info("call disconnected (BYE or timeout)");
            manager_.dispatch(inv, mod_id, CallDisconnected{});
        }
    }
}

} // namespace SIPI
