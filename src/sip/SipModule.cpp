#include "SipModule.hpp"

#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_dialog.h>
#include <pjsip/sip_util.h>
#include <pjsip/sip_ua_layer.h>

#include "CallManager.hpp"
#include "CallSession.hpp"
#include "Events.hpp"
#include "SipStatusCodes.hpp"
#include "utils/log.hpp"

namespace SIPI {

namespace {
// PJSIP stores C callbacks in pjsip_module/pjsip_inv_callback, so the static
// callback entry points need one tiny bridge back to our C++ object.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
SipModule* g_module = nullptr;
} // namespace

SipModule::SipModule(CallManager& manager)
    : manager_(manager) {
    g_module = this;

    mod_.name = {.ptr = name_.data(), .slen = static_cast<pj_ssize_t>(name_.size())};
    mod_.id = -1;
    mod_.priority = PJSIP_MOD_PRIORITY_APPLICATION;
    mod_.on_rx_request = &SipModule::on_rx_request;
}

SipModule::~SipModule() {
    if (g_module == this) {
        g_module = nullptr;
    }
}

void SipModule::set_endpoint(pjsip_endpoint* endpt) {
    endpoint_ = endpt;
}

pjsip_inv_callback SipModule::inv_callbacks() {
    pjsip_inv_callback cb{};
    // These callbacks are global for the INVITE usage, not per-call. Per-call
    // state is recovered through inv->mod_data[our module id].
    cb.on_state_changed = &SipModule::on_inv_state_changed;
    cb.on_media_update = &SipModule::on_inv_media_update;
    return cb;
}

void SipModule::on_inv_state_changed(pjsip_inv_session* inv, pjsip_event* /* ev */) {
    if (g_module == nullptr || g_module->mod_.id < 0) {
        return;
    }

    auto& self = *g_module;
    // mod_data is PJSIP's per-module storage slot on the INVITE session. We
    // store the owning CallSession there when the call is created.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    auto* session = static_cast<CallSession*>(inv->mod_data[self.mod_.id]);
    if (session == nullptr) {
        return;
    }

    if (inv->state == PJSIP_INV_STATE_CONFIRMED) {
        Log::sip()->debug("[{}] INVITE confirmed (ACK received)", session->call_id());
        session->dispatch(AckReceived{});
        return;
    }

    if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {
        Log::sip()->info("[{}] call terminated (BYE or timeout)", session->call_id());
        // Dispatch BYE to state machine so it can transition out cleanly
        session->dispatch(ByeReceived{});
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        inv->mod_data[self.mod_.id] = nullptr;
        self.manager_.remove(session->call_id());
    }
}

void SipModule::on_inv_media_update(pjsip_inv_session* /*inv*/, pj_status_t /*status*/) {}

pj_bool_t SipModule::on_rx_request(pjsip_rx_data* rdata) {
    if (g_module == nullptr) {
        return PJ_FALSE;
    }

    auto& self = *g_module;
    // This callback is for out-of-dialog requests. In-dialog ACK/BYE/etc are
    // normally consumed by the transaction/dialog/INVITE layers before here.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
    const pjsip_method& method = rdata->msg_info.msg->line.req.method;

    // Respond (statelessly) any non-INVITE requests with 500
    if (method.id != PJSIP_INVITE_METHOD) {
        if (method.id != PJSIP_ACK_METHOD) {
            auto reason_text = std::to_array("SIP UA unable to handle this request");
            pj_str_t reason = {.ptr = reason_text.data(), .slen = static_cast<pj_ssize_t>(reason_text.size() - 1)};
            pjsip_endpt_respond_stateless(self.endpoint_, rdata, kSipInternalError, &reason, nullptr, nullptr);
        }
        return PJ_TRUE;
    }

    if (rdata->msg_info.to != nullptr && rdata->msg_info.to->tag.slen > 0) {
        return PJ_FALSE;
    }

    if (self.mod_.id < 0 || self.endpoint_ == nullptr) {
        Log::sip()->warn("INVITE received but module not ready");
        return PJ_FALSE;
    }

    const std::string call_id(rdata->msg_info.cid->id.ptr, static_cast<std::size_t>(rdata->msg_info.cid->id.slen));
    Log::sip()->debug("[{}] INVITE received", call_id);

    if (self.manager_.find(call_id) != nullptr) {
        Log::sip()->warn("[{}] duplicate INVITE (call already exists)", call_id);
        return PJ_FALSE;
    }

    unsigned options = 0;
    pjsip_tx_data* verify_response = nullptr;

    // Let the INVITE usage validate required headers, supported methods, and
    // related protocol details before we create dialog state.
    pj_status_t status = pjsip_inv_verify_request(rdata, &options, nullptr, nullptr, self.endpoint_, &verify_response);
    if (status != PJ_SUCCESS) {
        Log::sip()->warn("[{}] INVITE verification failed (status={})", call_id, status);
        if (verify_response != nullptr) {
            if (pjsip_endpt_send_response2(self.endpoint_, rdata, verify_response, nullptr, nullptr) != PJ_SUCCESS) {
                Log::sip()->warn("[{}] failed to send verification response", call_id);
            }
        }
        else {
            pjsip_endpt_respond_stateless(self.endpoint_, rdata, kSipInternalError, nullptr, nullptr, nullptr);
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
        pjsip_endpt_respond_stateless(self.endpoint_, rdata, kSipInternalError, nullptr, nullptr, nullptr);
        return PJ_TRUE;
    }

    // Create INVITE session within the dialog; the inv layer will manage state and send responses.
    pjsip_inv_session* inv = nullptr;
    status = pjsip_inv_create_uas(dlg, rdata, nullptr, options, &inv);
    if (status != PJ_SUCCESS || inv == nullptr) {
        Log::sip()->warn("[{}] failed to create UAS inv session (status={})", call_id, status);
        pjsip_endpt_respond(self.endpoint_, &self.mod_, rdata, kSipInternalError, nullptr, nullptr, nullptr, nullptr);
        pjsip_dlg_dec_lock(dlg);
        return PJ_TRUE;
    }

    Log::sip()->debug("[{}] UAS dialog and inv session created", call_id);
    self.manager_.dispatch(InviteReceived{.inv_ = inv, .rdata_ = rdata}, self.mod_.id);

    // Release the dialog lock
    pjsip_dlg_dec_lock(dlg);
    return PJ_TRUE;
}

} // namespace SIPI
