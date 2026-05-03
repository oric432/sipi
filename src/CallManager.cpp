#include "CallManager.hpp"

#include "utils/log.hpp"

namespace SIPI {

void CallManager::dispatch(const InviteReceived& event, int mod_id) {
    const std::string id(event.rdata_->msg_info.cid->id.ptr,
                         static_cast<std::size_t>(event.rdata_->msg_info.cid->id.slen));

    if (sessions_.contains(id)) {
        Log::app()->warn("[{}] duplicate INVITE — ignoring", id);
        return;
    }

    auto session = std::make_unique<CallSession>(event);

    // Store raw pointer for O(1) routing of subsequent callbacks
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    event.inv_->mod_data[mod_id] = session.get();

    sessions_.emplace(id, std::move(session));
}

void CallManager::remove(std::string_view call_id) {
    if (sessions_.erase(std::string(call_id)) > 0) {
        Log::app()->info("[{}] call removed", call_id);
    }
}

} // namespace SIPI
