#include "CallContext.hpp"

#include <pjsip/sip_dialog.h>
#include <pjsip/sip_ua_layer.h>
#include <pjsip_ua.h>

#include "SipResponder.hpp"
#include "SipStatusCodes.hpp"
#include "utils/log.hpp"

namespace SIPI {

CallContext::CallContext(const IncomingInvite& ev, boost::asio::io_context& ioc, const Settings& settings)
    : rdata_(ev.rdata_)
    , endpt_(ev.endpoint_)
    , mod_id_(ev.mod_id_)
    , rtp_(std::string(ev.rdata_->msg_info.cid->id.ptr, static_cast<std::size_t>(ev.rdata_->msg_info.cid->id.slen)))
    , ioc_(ioc)
    , call_id_(ev.rdata_->msg_info.cid->id.ptr, static_cast<std::size_t>(ev.rdata_->msg_info.cid->id.slen))
    , port_min_(static_cast<uint16_t>(settings.get<int64_t>(Settings::Path::kRTP_PORT_MIN)))
    , port_max_(static_cast<uint16_t>(settings.get<int64_t>(Settings::Path::kRTP_PORT_MAX)))
    , public_ip_(settings.get<std::string>(Settings::Path::kSIP_PUBLIC_ADDRESS)) {}

bool CallContext::create_uas_invite_session() {
    unsigned options = 0;
    pjsip_tx_data* verify_response = nullptr;

    // Validate required headers and supported methods
    pj_status_t status = pjsip_inv_verify_request(rdata_, &options, nullptr, nullptr, endpt_, &verify_response);
    if (status != PJ_SUCCESS) {
        Log::sip()->warn("[{}] INVITE verification failed (status={})", call_id_, status);
        if (verify_response != nullptr) {
            if (pjsip_endpt_send_response2(endpt_, rdata_, verify_response, nullptr, nullptr) != PJ_SUCCESS) {
                Log::sip()->warn("[{}] failed to send verification response", call_id_);
            }
        }
        else {
            pjsip_endpt_respond_stateless(endpt_, rdata_, kSipInternalError, nullptr, nullptr, nullptr);
        }
        return false;
    }

    // Create UAS dialog from incoming INVITE
    pjsip_dialog* dlg = nullptr;
    status = pjsip_dlg_create_uas_and_inc_lock(pjsip_ua_instance(), rdata_, nullptr, &dlg);
    if (status != PJ_SUCCESS) {
        Log::sip()->warn("[{}] failed to create UAS dialog (status={})", call_id_, status);
        pjsip_endpt_respond_stateless(endpt_, rdata_, kSipInternalError, nullptr, nullptr, nullptr);
        return false;
    }

    // Create INVITE session within the dialog
    pjsip_inv_session* inv = nullptr;
    status = pjsip_inv_create_uas(dlg, rdata_, nullptr, options, &inv);
    if (status != PJ_SUCCESS || inv == nullptr) {
        Log::sip()->warn("[{}] failed to create UAS inv session (status={})", call_id_, status);
        pjsip_endpt_respond(endpt_, nullptr, rdata_, kSipInternalError, nullptr, nullptr, nullptr, nullptr);
        pjsip_dlg_dec_lock(dlg);
        return false;
    }

    // Initialize SdpNegotiator now that dialog pool exists
    negotiator_.emplace(dlg->pool);
    inv_ = inv;

    pjsip_dlg_dec_lock(dlg);
    Log::sip()->debug("[{}] UAS dialog and inv session created", call_id_);
    return true;
}

void CallContext::send_trying() {
    Log::sip()->debug("[{}] sending 100 Trying", call_id_);
    SipResponder::send_trying(inv_, rdata_);
}

std::string_view CallContext::extract_sdp_body() const {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
    if (rdata_ && rdata_->msg_info.msg->body && rdata_->msg_info.msg->body->data) {
        return {
            static_cast<const char*>(rdata_->msg_info.msg->body->data),
            static_cast<std::size_t>(rdata_->msg_info.msg->body->len)};
    }
    return {};
}

std::optional<SdpParsed> CallContext::parse_sdp() {
    Log::sip()->debug("[{}] parsing remote SDP", call_id_);
    if (!negotiator_) {
        return std::nullopt;
    }
    auto result = negotiator_->parse_remote(extract_sdp_body());
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
