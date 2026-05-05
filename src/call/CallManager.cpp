#include "CallManager.hpp"

#include "utils/log.hpp"

namespace SIPI {

CallManager::CallManager(boost::asio::io_context& ioc, const Settings& settings)
    : ioc_(ioc)
    , settings_(settings) {}

void CallManager::dispatch(const InviteReceived& event, int mod_id) {
    const std::string id(
        event.rdata_->msg_info.cid->id.ptr,
        static_cast<std::size_t>(event.rdata_->msg_info.cid->id.slen));

    Log::app()->debug("CallManager::dispatch INVITE [{}] - starting", id);

    if (sessions_.contains(id)) {
        Log::app()->warn("[{}] duplicate INVITE — ignoring", id);
        return;
    }

    Log::app()->debug("[{}] creating CallSession", id);
    auto session = std::make_unique<CallSession>(event, ioc_, settings_);
    Log::app()->debug("[{}] CallSession created successfully", id);

    // Store raw pointer for O(1) routing of subsequent callbacks
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    event.inv_->mod_data[mod_id] = session.get();
    Log::app()->debug("[{}] stored session pointer in mod_data", id);

    sessions_.emplace(id, std::move(session));
    Log::app()->debug("[{}] session stored in map", id);
}

void CallManager::remove(std::string_view call_id) {
    if (sessions_.erase(std::string(call_id)) > 0) {
        Log::app()->info("[{}] call removed", call_id);
    }
}

CallSession* CallManager::find(std::string_view call_id) {
    auto it = sessions_.find(std::string(call_id));
    return (it != sessions_.end()) ? it->second.get() : nullptr;
}

} // namespace SIPI
