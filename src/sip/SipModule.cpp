#include "SipModule.hpp"

#include "CallManager.hpp"

namespace SIPI {

namespace {
// PJSIP stores C callbacks in pjsip_module, so the static callback entry points
// need one tiny bridge back to our C++ object.
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

pjsip_inv_callback SipModule::inv_callbacks() {
    pjsip_inv_callback cb{};
    cb.on_state_changed = &SipModule::on_inv_state_changed;
    cb.on_media_update = &SipModule::on_inv_media_update;
    return cb;
}

void SipModule::on_inv_state_changed(pjsip_inv_session* inv, pjsip_event* /* ev */) {
    if (g_module == nullptr || g_module->mod_.id < 0) {
        return;
    }
    g_module->manager_.on_inv_state_changed(inv, g_module->mod_.id);
}

void SipModule::on_inv_media_update(pjsip_inv_session* /*inv*/, pj_status_t /*status*/) {}

pj_bool_t SipModule::on_rx_request(pjsip_rx_data* rdata) {
    if (g_module == nullptr || g_module->mod_.id < 0) {
        return PJ_FALSE;
    }
    return g_module->manager_.on_incoming_invite(rdata, g_module->endpt_, g_module->mod_.id);
}

} // namespace SIPI
