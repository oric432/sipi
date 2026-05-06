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

    // This callback is for out-of-dialog requests. In-dialog ACK/BYE/etc are
    // normally consumed by the transaction/dialog/INVITE layers before here.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
    const pjsip_method& method = rdata->msg_info.msg->line.req.method;

    // Respond (statelessly) any non-INVITE requests with 500
    if (method.id != PJSIP_INVITE_METHOD) {
        if (method.id != PJSIP_ACK_METHOD) {
            auto reason_text = std::to_array("SIP UA unable to handle this request");
            pj_str_t reason = {.ptr = reason_text.data(), .slen = static_cast<pj_ssize_t>(reason_text.size() - 1)};
            pjsip_endpt_respond_stateless(endpt_, rdata, kSipInternalError, &reason, nullptr, nullptr);
        }
        return PJ_TRUE;
    }

    if (rdata->msg_info.to != nullptr && rdata->msg_info.to->tag.slen > 0) {
        return PJ_FALSE;
    }

    const std::string call_id(rdata->msg_info.cid->id.ptr, static_cast<std::size_t>(rdata->msg_info.cid->id.slen));
    Log::sip()->debug("[{}] INVITE received", call_id);

    if (manager_.find(call_id) != nullptr) {
        Log::sip()->warn("[{}] duplicate INVITE (call already exists)", call_id);
        return PJ_FALSE;
    }

    unsigned options = 0;
    pjsip_tx_data* verify_response = nullptr;

    // Let the INVITE usage validate required headers, supported methods, and
    // related protocol details before we create dialog state.
    pj_status_t status = pjsip_inv_verify_request(rdata, &options, nullptr, nullptr, endpt_, &verify_response);
    if (status != PJ_SUCCESS) {
        Log::sip()->warn("[{}] INVITE verification failed (status={})", call_id, status);
        if (verify_response != nullptr) {
            if (pjsip_endpt_send_response2(endpt_, rdata, verify_response, nullptr, nullptr) != PJ_SUCCESS) {
                Log::sip()->warn("[{}] failed to send verification response", call_id);
            }
        }
        else {
            pjsip_endpt_respond_stateless(endpt_, rdata, kSipInternalError, nullptr, nullptr, nullptr);
        }
        return PJ_TRUE;
    }

    // Create UAS (User Agent Server) dialog from incoming INVITE.
    // pjsip_dlg_create_uas_and_inc_lock locks the dialog and increments its reference count;
    // we must call pjsip_dlg_dec_lock() after pjsip_inv_create_uas() to release the lock.
    pjsip_dialog* dlg = nullptr;
    status = pjsip_dlg_create_uas_and_inc_lock(pjsip_ua_instance(), rdata, nullptr, &dlg);
    if (status != PJ_SUCCESS) {
        Log::sip()->warn("[{}] failed to create UAS dialog (status={})", call_id, status);
        pjsip_endpt_respond_stateless(endpt_, rdata, kSipInternalError, nullptr, nullptr, nullptr);
        return PJ_TRUE;
    }

    // Create INVITE session within the dialog; the inv layer will manage state and send responses.
    pjsip_inv_session* inv = nullptr;
    status = pjsip_inv_create_uas(dlg, rdata, nullptr, options, &inv);
    if (status != PJ_SUCCESS || inv == nullptr) {
        Log::sip()->warn("[{}] failed to create UAS inv session (status={})", call_id, status);
        pjsip_endpt_respond(endpt_, nullptr, rdata, kSipInternalError, nullptr, nullptr, nullptr, nullptr);
        pjsip_dlg_dec_lock(dlg);
        return PJ_TRUE;
    }

    Log::sip()->debug("[{}] UAS dialog and inv session created", call_id);
    manager_.on_new_call(inv, rdata, mod_id);

    // Release the dialog lock
    pjsip_dlg_dec_lock(dlg);
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
