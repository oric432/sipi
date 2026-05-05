#pragma once

#include <queue>
#include <string>

#include <boost/asio/io_context.hpp>
#include <boost/sml.hpp>

#include "CallContext.hpp"
#include "CallStateMachine.hpp"
#include "Events.hpp"
#include "Settings.hpp"
#include "utils/log.hpp"

namespace SIPI {

struct CallStateLogger {
    template <class SM, class TEvent>
    void log_process_event(const TEvent& /*event*/) {
        // Log::call()->debug("[sml] event {}", typeid(TEvent).name());
    }

    template <class SM, class TGuard, class TEvent>
    void log_guard(const TGuard& /*guard*/, const TEvent& /*event*/, bool /*result*/) {
        // Log::call()->debug("[sml] guard {} -> {}", typeid(TGuard).name(), result);
    }

    template <class SM, class TAction, class TEvent>
    void log_action(const TAction& /*action*/, const TEvent& /*event*/) {
        // Log::call()->debug("[sml] action {}", typeid(TAction).name());
    }

    template <class SM, class TSrcState, class TDstState>
    void log_state_change(const TSrcState& src, const TDstState& dst) {
        Log::call()->debug("[sml] state {} -> {}", src.c_str(), dst.c_str());
    }
};

class CallSession {
public:
    explicit CallSession(const InviteReceived& event, boost::asio::io_context& ioc, const Settings& settings);
    ~CallSession() = default;

    CallSession(const CallSession&) = delete;
    CallSession(CallSession&&) = delete;
    CallSession& operator=(const CallSession&) = delete;
    CallSession& operator=(CallSession&&) = delete;

    [[nodiscard]] const std::string& call_id() const { return call_id_; }

    template <typename E>
    void dispatch(const E& event) {
        sm_.process_event(event);
    }

private:
    std::string call_id_;
    CallContext ctx_;
    CallStateLogger logger_;
    boost::sml::
        sm<CallStateMachine<CallContext>, boost::sml::process_queue<std::queue>, boost::sml::logger<CallStateLogger>>
            sm_;
};

} // namespace SIPI
