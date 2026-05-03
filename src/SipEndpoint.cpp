#include "SipEndpoint.hpp"

#include <csignal>

#include <pjlib-util.h>
#include <pjsip_ua.h>

#include "utils/log.hpp"

namespace SIPI {

namespace {
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<bool>* g_quit_flag = nullptr;

void on_sigint(int /*sig*/) {
    if (g_quit_flag != nullptr) {
        g_quit_flag->store(true, std::memory_order_relaxed);
    }
}
} // namespace

SipEndpoint::SipEndpoint(const Settings& settings)
    : module_(manager_)
{
    if (pj_init() != PJ_SUCCESS) {
        Log::crash_error("pj_init() failed");
    }

    if (pjlib_util_init() != PJ_SUCCESS) {
        Log::crash_error("pjlib_util_init() failed");
    }

    pj_caching_pool_init(&cp_, nullptr, 0);

    if (pjsip_endpt_create(&cp_.factory, nullptr, &endpt_) != PJ_SUCCESS) {
        Log::crash_error("pjsip_endpt_create() failed");
    }

    if (pjsip_ua_init_module(endpt_, nullptr) != PJ_SUCCESS) {
        Log::crash_error("pjsip_ua_init_module() failed");
    }

    auto inv_cb = SipModule::inv_callbacks();
    if (pjsip_inv_usage_init(endpt_, &inv_cb) != PJ_SUCCESS) {
        Log::crash_error("pjsip_inv_usage_init() failed");
    }

    const auto bind_addr   = settings.get<std::string>(Settings::Path::kSIP_BIND_ADDRESS);
    const auto bind_port   = settings.get<int64_t>(Settings::Path::kSIP_BIND_PORT);
    const auto public_addr = settings.get<std::string>(Settings::Path::kSIP_PUBLIC_ADDRESS);

    pj_str_t bind_str;
    pj_cstr(&bind_str, bind_addr.c_str());
    pj_sockaddr_in local{};
    pj_sockaddr_in_init(&local, &bind_str, static_cast<pj_uint16_t>(bind_port));

    pjsip_host_port a_name{};
    pj_cstr(&a_name.host, public_addr.c_str());
    a_name.port = static_cast<int>(bind_port);

    if (pjsip_udp_transport_start(endpt_, &local, &a_name, 1, nullptr) != PJ_SUCCESS) {
        Log::crash_error("pjsip_udp_transport_start() failed");
    }

    if (pjsip_endpt_register_module(endpt_, module_.pjmodule()) != PJ_SUCCESS) {
        Log::crash_error("pjsip_endpt_register_module() failed");
    }
}

SipEndpoint::~SipEndpoint() {
    if (endpt_ != nullptr) {
        pjsip_endpt_destroy(endpt_);
        endpt_ = nullptr;
    }
    pj_caching_pool_destroy(&cp_);
    pj_shutdown();
}

void SipEndpoint::run() {
    g_quit_flag = &quit_;
    if (std::signal(SIGINT, on_sigint) == SIG_ERR) {
        Log::crash_error("std::signal() failed");
    }

    Log::app()->info("SIP endpoint ready");

    static constexpr long kEventPollMs = 500;
    while (!quit_.load(std::memory_order_relaxed)) {
        pj_time_val timeout{0, kEventPollMs};
        pjsip_endpt_handle_events(endpt_, &timeout);
    }

    Log::app()->info("SIP endpoint stopping");
}

void SipEndpoint::stop() {
    quit_.store(true, std::memory_order_relaxed);
}

} // namespace SIPI
