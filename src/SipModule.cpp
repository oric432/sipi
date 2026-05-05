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
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
CallManager* g_call_manager = nullptr;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
pjsip_module* g_pjmodule = nullptr;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
pjsip_endpoint* g_endpoint = nullptr;

void on_inv_state_changed(pjsip_inv_session* inv, pjsip_event* e) {
    if (g_call_manager == nullptr || g_pjmodule == nullptr || g_pjmodule->id < 0) {
        return;
    }

    (void)e;
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
    g_pjmodule = &mod_;

    mod_.name = {.ptr = name_.data(), .slen = static_cast<pj_ssize_t>(name_.size())};
    mod_.id = -1;
    mod_.priority = PJSIP_MOD_PRIORITY_APPLICATION;
    mod_.on_rx_request = &SipModule::on_rx_request;
}

void SipModule::set_endpoint(pjsip_endpoint* endpt) {
    g_endpoint = endpt;
}

pjsip_inv_callback SipModule::inv_callbacks() {
    pjsip_inv_callback cb{};
    cb.on_state_changed = &on_inv_state_changed;
    cb.on_media_update = &on_inv_media_update;
    return cb;
}

pj_bool_t SipModule::on_rx_request(pjsip_rx_data* rdata) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
    const pjsip_method& method = rdata->msg_info.msg->line.req.method;

    // INVITE: create new session
    if (pjsip_method_cmp(&method, &pjsip_invite_method) == 0) {
        if (rdata->msg_info.to != nullptr && rdata->msg_info.to->tag.slen > 0) {
            return PJ_FALSE;
        }

        if (g_call_manager == nullptr || g_pjmodule == nullptr || g_endpoint == nullptr) {
            Log::app()->warn("on_rx_request(INVITE): module not ready");
            return PJ_FALSE;
        }

        // check if call already exists
        const std::string call_id(rdata->msg_info.cid->id.ptr, static_cast<std::size_t>(rdata->msg_info.cid->id.slen));
        if (g_call_manager->find(call_id) != nullptr) {
            Log::app()->debug("[{}] duplicate initial INVITE ignored", call_id);
            return PJ_FALSE;
        }

        // verify INVITE
        unsigned options = 0;
        pjsip_tx_data* verify_response = nullptr;

        pj_status_t status = pjsip_inv_verify_request(rdata, &options, nullptr, nullptr, g_endpoint, &verify_response);
        if (status != PJ_SUCCESS) {
            Log::app()->warn("[{}] INVITE verification failed: {}", call_id, status);
            if (verify_response != nullptr) {
                if (pjsip_endpt_send_response2(g_endpoint, rdata, verify_response, nullptr, nullptr) != PJ_SUCCESS) {
                    Log::app()->warn("[{}] failed to send INVITE verification response", call_id);
                }
            }
            else {
                pjsip_endpt_respond_stateless(g_endpoint, rdata, kSipInternalError, nullptr, nullptr, nullptr);
            }
            return PJ_TRUE;
        }

        // create dialog
        pjsip_dialog* dlg = nullptr;
        status = pjsip_dlg_create_uas_and_inc_lock(pjsip_ua_instance(), rdata, nullptr, &dlg);
        if (status != PJ_SUCCESS) {
            Log::app()->warn("[{}] failed to create UAS dialog: {}", call_id, status);
            pjsip_endpt_respond_stateless(g_endpoint, rdata, kSipInternalError, nullptr, nullptr, nullptr);
            return PJ_TRUE;
        }

        // create invite session
        pjsip_inv_session* inv = nullptr;
        status = pjsip_inv_create_uas(dlg, rdata, nullptr, options, &inv);
        if (status != PJ_SUCCESS || inv == nullptr) {
            Log::app()->warn("[{}] failed to create UAS invite session: {}", call_id, status);
            pjsip_endpt_respond(g_endpoint, g_pjmodule, rdata, kSipInternalError, nullptr, nullptr, nullptr, nullptr);
            pjsip_dlg_dec_lock(dlg);
            return PJ_TRUE;
        }

        // dispatch to call manager
        g_call_manager->dispatch(InviteReceived{.inv_ = inv, .rdata_ = rdata}, g_pjmodule->id);
        pjsip_dlg_dec_lock(dlg);
        return PJ_TRUE;
    }

    // BYE: dispatch to existing session
    if (pjsip_method_cmp(&method, &pjsip_bye_method) == 0) {
        if (g_call_manager != nullptr && g_pjmodule != nullptr) {
            std::string call_id(rdata->msg_info.cid->id.ptr, static_cast<std::size_t>(rdata->msg_info.cid->id.slen));
            if (auto* session = g_call_manager->find(call_id)) {
                session->dispatch(ByeReceived{});
            }
        }
        return PJ_FALSE;
    }

    // CANCEL: dispatch to existing session
    if (pjsip_method_cmp(&method, &pjsip_cancel_method) == 0) {
        if (g_call_manager != nullptr && g_pjmodule != nullptr) {
            std::string call_id(rdata->msg_info.cid->id.ptr, static_cast<std::size_t>(rdata->msg_info.cid->id.slen));
            if (auto* session = g_call_manager->find(call_id)) {
                session->dispatch(CancelReceived{});
            }
        }
        return PJ_FALSE;
    }

    return PJ_FALSE;
}

} // namespace SIPI
