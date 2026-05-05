#pragma once

#include <atomic>
#include <thread>

#include <boost/asio/io_context.hpp>

#include "CallManager.hpp"
#include "Settings.hpp"
#include "SipModule.hpp"

namespace SIPI {

class SipEndpoint {
public:
    explicit SipEndpoint(const Settings& settings);
    ~SipEndpoint();

    SipEndpoint(const SipEndpoint&) = delete;
    SipEndpoint(SipEndpoint&&) = delete;
    SipEndpoint& operator=(const SipEndpoint&) = delete;
    SipEndpoint& operator=(SipEndpoint&&) = delete;

    void run();
    void stop();

private:
    boost::asio::io_context ioc_;
    const Settings& settings_;
    pj_caching_pool cp_{};
    pjsip_endpoint* endpt_{};
    CallManager manager_;
    SipModule module_;
    std::thread asio_thread_;
    std::atomic<bool> quit_{false};
};

} // namespace SIPI
