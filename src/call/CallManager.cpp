#include "CallManager.hpp"

#include "CallSession.hpp"
#include "Events.hpp"
#include "utils/log.hpp"

namespace SIPI {

CallManager::CallManager(boost::asio::io_context& ioc, const Settings& settings)
    : ioc_(ioc)
    , settings_(settings) {}

void CallManager::on_incoming_invite(pjsip_rx_data* rdata, pjsip_endpoint* endpt, int mod_id) {
    const std::string id(rdata->msg_info.cid->id.ptr, static_cast<std::size_t>(rdata->msg_info.cid->id.slen));

    // Check for duplicate INVITE
    if (find(id) != nullptr) {
        Log::call()->warn("[{}] duplicate INVITE, ignoring", id);
        return;
    }

    // Create session with raw INVITE data — SM will create PJSIP dialog/inv on first event
    auto result = CallSession::make(
        IncomingInvite{.rdata_ = rdata, .endpoint_ = endpt, .mod_id_ = mod_id},
        ioc_, settings_);

    if (!result) {
        // Setup failed: SM already sent error response. Discard session.
        Log::call()->warn("[{}] CallSession setup failed", id);
        return;
    }

    auto session = std::move(*result);

    // By now, SM has processed the IncomingInvite and queued events (SetupOk/SetupFailed, SdpParsed, etc.)
    // Store raw pointer in PJSIP mod_data for O(1) callback routing by inv layer.
    auto* inv = session->inv();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    inv->mod_data[mod_id] = session.get();

    sessions_.emplace(id, std::move(session));
    Log::call()->info("[{}] new call session created", id);
}

void CallManager::on_inv_state_changed(pjsip_inv_session* inv, int mod_id) {
    if (inv == nullptr || mod_id < 0) {
        return;
    }

    if (inv->state == PJSIP_INV_STATE_CONFIRMED) {
        Log::sip()->debug("INVITE confirmed (ACK received)");
        dispatch(inv, mod_id, AckReceived{});
        return;
    }

    if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {
        if (inv->cancelling != 0) {
            Log::sip()->info("call cancelled");
            dispatch(inv, mod_id, CancelReceived{});
        }
        else {
            Log::sip()->info("call disconnected (BYE or timeout)");
            dispatch(inv, mod_id, CallDisconnected{});
        }
    }
}

CallSession* CallManager::find(std::string_view call_id) {
    auto it = sessions_.find(std::string(call_id));
    return (it != sessions_.end()) ? it->second.get() : nullptr;
}

CallSession* CallManager::find(pjsip_inv_session* inv, int mod_id) {
    if (inv == nullptr || mod_id < 0) {
        return nullptr;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    return static_cast<CallSession*>(inv->mod_data[mod_id]);
}

void CallManager::dispatch(pjsip_inv_session* inv, int mod_id, const AckReceived& event) {
    auto* session = find(inv, mod_id);
    if (session != nullptr) {
        session->dispatch(event);
    }
}

void CallManager::dispatch(pjsip_inv_session* inv, int mod_id, const CancelReceived& event) {
    auto* session = find(inv, mod_id);
    if (session != nullptr) {
        session->dispatch(event);
    }
}

void CallManager::dispatch(pjsip_inv_session* inv, int mod_id, const CallDisconnected& event) {
    auto* session = find(inv, mod_id);
    if (session != nullptr) {
        session->dispatch(event);
        // Terminal event: cleanup PJSIP state and remove session.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        inv->mod_data[mod_id] = nullptr;
        remove(session->call_id());
    }
}

void CallManager::remove(std::string_view call_id) {
    const std::string id{call_id};
    if (sessions_.erase(id) > 0) {
        Log::call()->info("[{}] call removed", id);
    }
}

} // namespace SIPI
