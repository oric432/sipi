#pragma once

#include <string>

#include <pjsip.h>

namespace SIPI {

class SipModule {
public:
    SipModule();
    pjsip_module* pjmodule() { return &mod_; }

private:
    static pj_bool_t on_rx_request(pjsip_rx_data* rdata);

    std::string  name_{"sipi-module"};
    pjsip_module mod_{};
};

} // namespace SIPI
