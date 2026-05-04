#pragma once

#include <queue>
#include <string>

#include <boost/asio/io_context.hpp>
#include <boost/sml.hpp>

#include "CallContext.hpp"
#include "CallStateMachine.hpp"
#include "Events.hpp"
#include "Settings.hpp"

namespace SIPI {

class CallSession {
public:
    explicit CallSession(const InviteReceived& event, boost::asio::io_context& ioc,
                         const Settings& settings);
    ~CallSession() = default;

    CallSession(const CallSession&)            = delete;
    CallSession(CallSession&&)                 = delete;
    CallSession& operator=(const CallSession&) = delete;
    CallSession& operator=(CallSession&&)      = delete;

    [[nodiscard]] const std::string& call_id() const { return call_id_; }

    template<typename E>
    void dispatch(const E& event) {
        sm_.process_event(event);
    }

private:
    std::string call_id_;
    CallContext ctx_;
    boost::sml::sm<CallStateMachine, boost::sml::process_queue<std::queue>> sm_;
};

} // namespace SIPI
