#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include <boost/asio/io_context.hpp>
#include <pjsip.h>

#include "CallSession.hpp"
#include "Events.hpp"
#include "Settings.hpp"

namespace SIPI {

class CallManager {
public:
    explicit CallManager(boost::asio::io_context& ioc, const Settings& settings);

    pj_bool_t on_incoming_invite(pjsip_rx_data* rdata, pjsip_endpoint* endpt, int mod_id);
    void on_inv_state_changed(pjsip_inv_session* inv, int mod_id);
    CallSession* find(std::string_view call_id);
    CallSession* find(pjsip_inv_session* inv, int mod_id);

    template <typename Event>
    void dispatch(pjsip_inv_session* inv, int mod_id, const Event& event) {
        auto* session = find(inv, mod_id);
        if (session != nullptr) {
            session->dispatch(event);
        }
    }

    template <>
    void dispatch<CallDisconnected>(pjsip_inv_session* inv, int mod_id, const CallDisconnected& event) {
        auto* session = find(inv, mod_id);
        if (session != nullptr) {
            session->dispatch(event);
            // Terminal event: cleanup PJSIP state and remove session.
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            inv->mod_data[mod_id] = nullptr;
            remove(session->call_id());
        }
    }

private:
    void remove(std::string_view call_id);
    // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
    boost::asio::io_context& ioc_;
    const Settings& settings_;
    // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)

    std::unordered_map<std::string, std::unique_ptr<CallSession>> sessions_;
};

} // namespace SIPI
