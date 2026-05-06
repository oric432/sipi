#include "CallContext.hpp"

#include <pjsip_ua.h>

#include "SipResponder.hpp"
#include "SipStatusCodes.hpp"
#include "utils/log.hpp"

namespace SIPI {

CallContext::CallContext(const InviteReceived& ev, boost::asio::io_context& ioc, const Settings& settings)
    : inv_(ev.inv_)
    , initial_rdata_(ev.rdata_)
    , call_id_(ev.rdata_->msg_info.cid->id.ptr, static_cast<std::size_t>(ev.rdata_->msg_info.cid->id.slen))
    // SDP built by the negotiator is handed back to PJSIP, so allocate its
    // PJ objects from the dialog pool that lives for the call.
    , negotiator_(ev.inv_->dlg->pool)
    , rtp_(call_id_)
    , ioc_(ioc)
    , port_min_(static_cast<uint16_t>(settings.get<int64_t>(Settings::Path::kRTP_PORT_MIN)))
    , port_max_(static_cast<uint16_t>(settings.get<int64_t>(Settings::Path::kRTP_PORT_MAX)))
    , public_ip_(settings.get<std::string>(Settings::Path::kSIP_PUBLIC_ADDRESS)) {}

void CallContext::send_trying() {
    Log::sip()->debug("[{}] sending 100 Trying", call_id_);
    SipResponder::send_trying(inv_, initial_rdata_);
}

std::string_view CallContext::extract_sdp_body() const {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
    if (initial_rdata_ && initial_rdata_->msg_info.msg->body && initial_rdata_->msg_info.msg->body->data) {
        return {
            static_cast<const char*>(initial_rdata_->msg_info.msg->body->data),
            static_cast<std::size_t>(initial_rdata_->msg_info.msg->body->len)};
    }
    return {};
}

std::optional<SdpParsed> CallContext::parse_sdp() {
    Log::sip()->debug("[{}] parsing remote SDP", call_id_);
    auto result = negotiator_.parse_remote(extract_sdp_body());
    if (!result) {
        return std::nullopt;
    }
    return SdpParsed{
        .remote_ip_ = result->ip_,
        .remote_port_ = result->port_,
        .has_audio_ = true,
        .supports_pcma_ = true,
    };
}

bool CallContext::open_rtp() {
    auto port_result = rtp_.open(ioc_, port_min_, port_max_);
    if (!port_result) {
        Log::rtp()->warn("[{}] open_rtp() failed", call_id_);
        return false;
    }
    uint16_t bound_port = *port_result;
    local_sdp_ = SdpNegotiator::build_local(public_ip_, bound_port);
    Log::rtp()->debug("[{}] open_rtp() bound to port {}", call_id_, bound_port);
    return true;
}

void CallContext::send_ringing() {
    Log::sip()->debug("[{}] sending 180 Ringing", call_id_);
    SipResponder::send_ringing(inv_);
}

void CallContext::send_ok() {
    Log::sip()->debug("[{}] sending 200 OK with SDP", call_id_);
    SipResponder::send_ok(inv_, local_sdp_);
}

void CallContext::send_reject(int code) {
    Log::sip()->warn("[{}] rejecting call with {}", call_id_, code);
    if (code == kSipRequestTerminated) {
        SipResponder::send_request_terminated(inv_);
    }
    else if (code == kSipNotAcceptableHere) {
        SipResponder::send_not_acceptable(inv_);
    }
    else {
        pjsip_tx_data* tdata = nullptr;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
        if (pjsip_inv_answer(inv_, code, nullptr, nullptr, &tdata) == PJ_SUCCESS && tdata != nullptr) {
            pjsip_inv_send_msg(inv_, tdata);
        }
    }
}

void CallContext::close_rtp() {
    Log::rtp()->debug("[{}] closing RTP session", call_id_);
    rtp_.close();
}

void CallContext::send_bye_ok() {
    // No-op: inv layer sends 200 OK to BYE when on_rx_request returns PJ_FALSE.
    Log::sip()->debug("[{}] BYE handled by inv layer (200 OK auto-sent)", call_id_);
}

} // namespace SIPI
