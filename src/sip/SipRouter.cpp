#include "SipRouter.hpp"

#include "CallManager.hpp"

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

    // All validation, routing, and business logic handled by CallManager
    manager_.on_incoming_invite(rdata, endpt_, mod_id);
    return PJ_TRUE;
}

void SipRouter::on_inv_state_changed(pjsip_inv_session* inv, int mod_id) {
    // All state translation and routing handled by CallManager
    manager_.on_inv_state_changed(inv, mod_id);
}

} // namespace SIPI
