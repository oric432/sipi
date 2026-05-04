#include "SipModule.hpp"

#include <pjsip/sip_dialog.h>
#include <pjsip/sip_ua_layer.h>

#include "CallManager.hpp"
#include "CallSession.hpp"
#include "Events.hpp"
#include "utils/log.hpp"

namespace SIPI {

namespace {
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
CallManager* g_call_manager = nullptr;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
pjsip_module* g_pjmodule = nullptr;

void on_inv_state_changed(pjsip_inv_session* inv, pjsip_event* e) {
    if (g_call_manager == nullptr || g_pjmodule == nullptr || g_pjmodule->id < 0) {
        return;
    }

    // For incoming INVITE, create a new session
    if (inv->state == PJSIP_INV_STATE_INCOMING) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        if (inv->mod_data[g_pjmodule->id] != nullptr) {
            return;  // Already created
        }

        // Get rdata from the event
        if (e == nullptr || e->type != PJSIP_EVENT_RX_MSG) {
            return;
        }
        pjsip_rx_data* rdata = e->body.rx_msg.rdata;

        // Create the session
        g_call_manager->dispatch(InviteReceived{.inv_ = inv, .rdata_ = rdata}, g_pjmodule->id);
        return;
    }

    // For other states, retrieve existing session
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    auto* session = static_cast<CallSession*>(inv->mod_data[g_pjmodule->id]);
    if (session == nullptr) {
        return;
    }

    if (inv->state == PJSIP_INV_STATE_CONFIRMED) {
        session->dispatch(AckReceived{});
        return;
    }

    if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        inv->mod_data[g_pjmodule->id] = nullptr;
        g_call_manager->remove(session->call_id());
    }
}

void on_inv_media_update(pjsip_inv_session* /*inv*/, pj_status_t /*status*/) {}
} // namespace

SipModule::SipModule(CallManager& manager) {
    g_call_manager = &manager;
    g_pjmodule     = &mod_;

    mod_.name          = {.ptr = name_.data(), .slen = static_cast<pj_ssize_t>(name_.size())};
    mod_.id            = -1;
    mod_.priority      = PJSIP_MOD_PRIORITY_APPLICATION;
    mod_.on_rx_request = &SipModule::on_rx_request;
}

pjsip_inv_callback SipModule::inv_callbacks() {
    pjsip_inv_callback cb{};
    cb.on_state_changed = &on_inv_state_changed;
    cb.on_media_update  = &on_inv_media_update;
    return cb;
}

pj_bool_t SipModule::on_rx_request(pjsip_rx_data* rdata) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
    const pjsip_method& method = rdata->msg_info.msg->line.req.method;

    // INVITE: let the inv layer handle it (return PJ_TRUE to indicate we'll handle it)
    if (method.id == PJSIP_INVITE_METHOD) {
        return PJ_TRUE;
    }

    // BYE request: dispatch to session if exists
    if (method.id == PJSIP_BYE_METHOD) {
        if (g_call_manager != nullptr && g_pjmodule != nullptr) {
            std::string call_id(rdata->msg_info.cid->id.ptr,
                               static_cast<std::size_t>(rdata->msg_info.cid->id.slen));
            if (auto* session = g_call_manager->find(call_id)) {
                session->dispatch(ByeReceived{});
            }
        }
        return PJ_FALSE;
    }

    // CANCEL request: dispatch to session if exists
    if (method.id == PJSIP_CANCEL_METHOD) {
        if (g_call_manager != nullptr && g_pjmodule != nullptr) {
            std::string call_id(rdata->msg_info.cid->id.ptr,
                               static_cast<std::size_t>(rdata->msg_info.cid->id.slen));
            if (auto* session = g_call_manager->find(call_id)) {
                session->dispatch(CancelReceived{});
            }
        }
        return PJ_FALSE;
    }

    // Don't handle other requests
    return PJ_FALSE;
}

} // namespace SIPI
