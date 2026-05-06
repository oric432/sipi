#include "CallSession.hpp"

#include "utils/error.hpp"
#include "utils/log.hpp"

namespace SIPI {

Error::Result<std::unique_ptr<CallSession>>
CallSession::make(const IncomingInvite& event, boost::asio::io_context& ioc, const Settings& settings) {
    auto session = std::unique_ptr<CallSession>(new CallSession(event, ioc, settings));

    if (session->inv() == nullptr) {
        return std::unexpected(Error::make_error().with_context("CallSession setup failed: inv session not created"));
    }

    return session;
}

CallSession::CallSession(const IncomingInvite& event, boost::asio::io_context& ioc, const Settings& settings)
    : ctx_(event, ioc, settings)
    , sm_(ctx_, logger_) {
    Log::call()->debug("[{}] CallSession created, about to process event", call_id());
    sm_.process_event(event);
    Log::call()->debug("[{}] CallSession event processed", call_id());
}

} // namespace SIPI
