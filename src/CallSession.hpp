#pragma once

#include <string>

#include "Events.hpp"

namespace SIPI {

class CallSession {
public:
    explicit CallSession(const InviteReceived& event);
    ~CallSession() = default;

    CallSession(const CallSession&)            = delete;
    CallSession(CallSession&&)                 = delete;
    CallSession& operator=(const CallSession&) = delete;
    CallSession& operator=(CallSession&&)      = delete;

    [[nodiscard]] const std::string& call_id() const { return call_id_; }

private:
    std::string        call_id_;
    pjsip_inv_session* inv_;
};

} // namespace SIPI
