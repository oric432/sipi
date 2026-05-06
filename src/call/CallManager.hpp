#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include <boost/asio/io_context.hpp>
#include <pjsip.h>

#include "CallSession.hpp"
#include "Settings.hpp"

namespace SIPI {

class CallManager {
public:
    explicit CallManager(boost::asio::io_context& ioc, const Settings& settings);

    void on_new_call(pjsip_inv_session* inv, pjsip_rx_data* rdata, int mod_id);
    void remove(std::string_view call_id);
    CallSession* find(std::string_view call_id);

private:
    // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
    boost::asio::io_context& ioc_;
    const Settings& settings_;
    // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)

    std::unordered_map<std::string, std::unique_ptr<CallSession>> sessions_;
};

} // namespace SIPI
