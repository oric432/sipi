#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include <boost/asio/io_context.hpp>

#include "CallSession.hpp"
#include "Events.hpp"
#include "Settings.hpp"

namespace SIPI {

class CallManager {
public:
    explicit CallManager(boost::asio::io_context& ioc, const Settings& settings);

    void dispatch(const InviteReceived& event, int mod_id);
    void remove(std::string_view call_id);
    CallSession* find(std::string_view call_id);

private:
    boost::asio::io_context& ioc_;
    const Settings& settings_;
    std::unordered_map<std::string, std::unique_ptr<CallSession>> sessions_;
};

} // namespace SIPI
