#pragma once

#include <pjsip.h>
#include <pjsip_ua.h>

namespace SIPI {

class CallManager;

class SipRouter {
public:
    explicit SipRouter(CallManager& manager);

    void set_endpoint(pjsip_endpoint* endpt);

    pj_bool_t on_request(pjsip_rx_data* rdata, int mod_id);
    void on_inv_state_changed(pjsip_inv_session* inv, int mod_id);

private:
    // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
    CallManager& manager_;
    // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)
    pjsip_endpoint* endpt_{};
};

} // namespace SIPI
