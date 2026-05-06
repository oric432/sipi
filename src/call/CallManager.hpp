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

    void on_incoming_invite(pjsip_rx_data* rdata, pjsip_endpoint* endpt, int mod_id);
    CallSession* find(std::string_view call_id);
    CallSession* find(pjsip_inv_session* inv, int mod_id);
    void dispatch(pjsip_inv_session* inv, int mod_id, const AckReceived& event);
    void dispatch(pjsip_inv_session* inv, int mod_id, const CancelReceived& event);
    void dispatch(pjsip_inv_session* inv, int mod_id, const CallDisconnected& event);

private:
    void remove(std::string_view call_id);
    // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
    boost::asio::io_context& ioc_;
    const Settings& settings_;
    // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)

    std::unordered_map<std::string, std::unique_ptr<CallSession>> sessions_;
};

} // namespace SIPI
