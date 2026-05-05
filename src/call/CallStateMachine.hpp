#pragma once

#include <string_view>

#include <boost/sml.hpp>

#include "Events.hpp"
#include "SipStatusCodes.hpp"

namespace SIPI {

struct Idle {};
struct IncomingInvite {};
struct Trying {};
struct Answered {};
struct Confirmed {};
struct Terminating {};
struct Failed {};

template <typename TContext>
struct CallStateMachine {
    auto operator()() const noexcept {
        using namespace boost::sml;

        // Guards
        auto is_valid_invite = [](const InviteReceived& ev) { return ev.inv_ != nullptr; };
        auto valid_sdp = [](const SdpParsed& ev) { return ev.has_audio_ && ev.supports_pcma_; };
        auto invalid_sdp = [](const SdpParsed& ev) { return !ev.has_audio_ || !ev.supports_pcma_; };

        // Actions
        auto invite_action =
            [](const InviteReceived& ev, TContext& ctx, back::process<SdpParsed, SdpRejected> process) {
                ctx.send_trying();
                std::string_view sdp_body;
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
                if (ev.rdata_ && ev.rdata_->msg_info.msg->body && ev.rdata_->msg_info.msg->body->data) {
                    sdp_body = std::string_view(
                        static_cast<const char*>(ev.rdata_->msg_info.msg->body->data),
                        static_cast<std::size_t>(ev.rdata_->msg_info.msg->body->len));
                }
                if (auto result = ctx.parse_sdp(sdp_body)) {
                    process(*result);
                }
                else {
                    process(SdpRejected{});
                }
            };

        auto open_rtp_action = [](TContext& ctx, back::process<RtpReady, TransportError> process) {
            if (ctx.open_rtp()) {
                process(RtpReady{});
            }
            else {
                process(TransportError{});
            }
        };

        auto answer_action = [](TContext& ctx) {
            ctx.send_ringing();
            ctx.send_ok();
        };

        auto reject_action = [](TContext& ctx) { ctx.send_reject(kSipNotAcceptableHere); };

        auto reject_transport_action = [](TContext& ctx) { ctx.send_reject(kSipInternalError); };

        auto bye_action = [](TContext& ctx) {
            ctx.close_rtp();
            ctx.send_bye_ok();
        };

        auto cancel_action = [](TContext& ctx) { ctx.close_rtp(); };

        // clang-format off
        return make_transition_table(
            *state<Idle>          + (event<InviteReceived> [is_valid_invite] / invite_action)           = state<IncomingInvite>,
            state<IncomingInvite> + (event<SdpParsed>      [valid_sdp]       / open_rtp_action)         = state<Trying>,
            state<IncomingInvite> + (event<SdpParsed>      [invalid_sdp]     / reject_action)           = state<Failed>,
            state<IncomingInvite> + (event<SdpRejected>                      / reject_action)           = state<Failed>,
            state<Trying>         + (event<RtpReady>                         / answer_action)           = state<Answered>,
            state<Trying>         + (event<TransportError>                   / reject_transport_action) = state<Failed>,
            state<Answered>       + event<AckReceived>                                                   = state<Confirmed>,
            state<Answered>       + (event<CancelReceived>                   / cancel_action)           = state<Terminating>,
            state<Confirmed>      + (event<ByeReceived>                      / bye_action)              = state<Terminating>,
            state<Confirmed>      + (event<CancelReceived>                   / cancel_action)           = state<Terminating>,
            state<Terminating>                                                                           = X
        );
        // clang-format on
    }
};

} // namespace SIPI
