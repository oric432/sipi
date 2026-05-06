#include "CallManager.hpp"

#include "CallSession.hpp"
#include "Events.hpp"
#include "utils/log.hpp"

namespace SIPI {

CallManager::CallManager(boost::asio::io_context& ioc, const Settings& settings)
    : ioc_(ioc)
    , settings_(settings) {}

void CallManager::on_new_call(pjsip_inv_session* inv, pjsip_rx_data* rdata, int mod_id) {
    const std::string id(
        rdata->msg_info.cid->id.ptr,
        static_cast<std::size_t>(rdata->msg_info.cid->id.slen));

    auto session = std::make_unique<CallSession>(InviteReceived{.inv_ = inv, .rdata_ = rdata}, ioc_, settings_);

    // Store raw pointer in PJSIP mod_data for O(1) callback routing by inv layer.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    inv->mod_data[mod_id] = session.get();

    sessions_.emplace(id, std::move(session));
    Log::call()->info("[{}] new call session created", id);
}

void CallManager::remove(std::string_view call_id) {
    const std::string id{call_id};
    if (sessions_.erase(id) > 0) {
        Log::call()->info("[{}] call removed", id);
    }
}

CallSession* CallManager::find(std::string_view call_id) {
    auto it = sessions_.find(std::string(call_id));
    return (it != sessions_.end()) ? it->second.get() : nullptr;
}

} // namespace SIPI
