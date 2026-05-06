#pragma once

#include <string>

#include <pjsip.h>
#include <pjsip_ua.h>

namespace SIPI {

class CallManager;

class SipModule {
public:
    explicit SipModule(CallManager& manager);
    ~SipModule();

    SipModule(const SipModule&) = delete;
    SipModule(SipModule&&) = delete;
    SipModule& operator=(const SipModule&) = delete;
    SipModule& operator=(SipModule&&) = delete;

    pjsip_module* pjmodule() { return &mod_; }
    void set_endpoint(pjsip_endpoint* endpt) { endpt_ = endpt; }
    [[nodiscard]] static pjsip_inv_callback inv_callbacks();

private:
    static pj_bool_t on_rx_request(pjsip_rx_data* rdata);
    static void on_inv_state_changed(pjsip_inv_session* inv, pjsip_event* ev);
    static void on_inv_media_update(pjsip_inv_session* inv, pj_status_t status);

    // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
    CallManager& manager_;
    // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)

    pjsip_endpoint* endpt_{};
    std::string name_{"sipi-module"};
    pjsip_module mod_{};
};

} // namespace SIPI
