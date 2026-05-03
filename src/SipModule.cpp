#include "SipModule.hpp"

namespace SIPI {

SipModule::SipModule() {
    mod_.name          = {.ptr = name_.data(), .slen = static_cast<pj_ssize_t>(name_.size())};
    mod_.id            = -1;
    mod_.priority      = PJSIP_MOD_PRIORITY_APPLICATION;
    mod_.on_rx_request = &SipModule::on_rx_request;
}

pj_bool_t SipModule::on_rx_request(pjsip_rx_data* /*rdata*/) {
    return PJ_FALSE;
}

} // namespace SIPI
