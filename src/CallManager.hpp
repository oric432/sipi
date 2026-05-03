#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "CallSession.hpp"
#include "Events.hpp"

namespace SIPI {

class CallManager {
public:
    void dispatch(const InviteReceived& event, int mod_id);
    void remove(std::string_view call_id);

private:
    std::unordered_map<std::string, std::unique_ptr<CallSession>> sessions_;
};

} // namespace SIPI
