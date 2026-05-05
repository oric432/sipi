#pragma once

#include <string>

#include <pjsip.h>
#include <pjsip_ua.h>

namespace SIPI {

class CallManager;

class SipModule {
public:
    explicit SipModule(CallManager& manager);
    void               set_endpoint(pjsip_endpoint* endpt);
    pjsip_module*      pjmodule() { return &mod_; }
    [[nodiscard]] static pjsip_inv_callback inv_callbacks();

private:
    static pj_bool_t on_rx_request(pjsip_rx_data* rdata);

    std::string  name_{"sipi-module"};
    pjsip_module mod_{};
};

} // namespace SIPI
