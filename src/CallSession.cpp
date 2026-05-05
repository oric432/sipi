#include "CallSession.hpp"

#include "utils/log.hpp"

namespace SIPI {

CallSession::CallSession(const InviteReceived& event, boost::asio::io_context& ioc, const Settings& settings)
    : call_id_(event.rdata_->msg_info.cid->id.ptr, static_cast<std::size_t>(event.rdata_->msg_info.cid->id.slen))
    , ctx_(event, ioc, settings)
    , sm_(static_cast<ICallContext&>(ctx_)) {
    Log::app()->debug("[{}] CallSession created, about to process event", call_id_);
    sm_.process_event(event);
    Log::app()->debug("[{}] CallSession event processed", call_id_);
}

} // namespace SIPI
