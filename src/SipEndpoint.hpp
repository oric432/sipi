#pragma once

#include <atomic>

#include "Settings.hpp"
#include "SipModule.hpp"

namespace SIPI {

class SipEndpoint {
public:
    explicit SipEndpoint(const Settings& settings);
    ~SipEndpoint();

    SipEndpoint(const SipEndpoint&)            = delete;
    SipEndpoint(SipEndpoint&&)                 = delete;
    SipEndpoint& operator=(const SipEndpoint&) = delete;
    SipEndpoint& operator=(SipEndpoint&&)      = delete;

    void run();
    void stop();

private:
    pj_caching_pool   cp_{};
    pjsip_endpoint*   endpt_{};
    SipModule         module_;
    std::atomic<bool> quit_{false};
};

} // namespace SIPI
