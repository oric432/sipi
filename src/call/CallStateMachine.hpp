#pragma once

#include <boost/sml.hpp>

#include "Events.hpp"
#include "SipStatusCodes.hpp"

namespace SIPI {

struct Idle {};
struct Accepting {}; // Between raw INVITE and dialog creation
struct DialogCreated {}; // Dialog exists; waiting for SDP (was IncomingInvite)
struct Trying {};
struct Answered {};
struct Confirmed {};
struct Terminating {};
struct Failed {};

// Templated for testing purposes
template <typename TContext>
struct CallStateMachine {
    auto operator()() const noexcept {
        using namespace boost::sml;

        // Guards
        auto valid_sdp = [](const SdpParsed& ev) { return ev.has_audio_ && ev.supports_pcma_; };
        auto invalid_sdp = [](const SdpParsed& ev) { return !ev.has_audio_ || !ev.supports_pcma_; };

        // Actions
        auto create_uas_action =
            [](const IncomingInvite& /*ev*/, TContext& ctx, back::process<SetupOk, SetupFailed> process) {
                if (ctx.create_uas_invite_session()) {
                    process(SetupOk{});
                }
                else {
                    process(SetupFailed{});
                }
            };

        auto invite_action = [](TContext& ctx, back::process<SdpParsed, SdpRejected> process) {
            ctx.send_trying();
            if (auto result = ctx.parse_sdp()) {
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

        auto disconnect_action = [](TContext& ctx) { ctx.close_rtp(); };

        auto cancel_action = [](TContext& ctx) { ctx.close_rtp(); };

        // clang-format off
        return make_transition_table(
            *state<Idle>          + (event<IncomingInvite>                   / create_uas_action)       = state<Accepting>,
            state<Accepting>      + (event<SetupOk>                          / invite_action)           = state<DialogCreated>,
            state<Accepting>      +  event<SetupFailed>                                                  = X,
            state<DialogCreated>  + (event<SdpParsed>      [valid_sdp]       / open_rtp_action)         = state<Trying>,
            state<DialogCreated>  + (event<SdpParsed>      [invalid_sdp]     / reject_action)           = state<Failed>,
            state<DialogCreated>  + (event<SdpRejected>                      / reject_action)           = state<Failed>,
            state<DialogCreated>  + (event<CancelReceived>                   / cancel_action)           = state<Terminating>,
            state<Trying>         + (event<RtpReady>                         / answer_action)           = state<Answered>,
            state<Trying>         + (event<TransportError>                   / reject_transport_action) = state<Failed>,
            state<Trying>         + (event<CancelReceived>                   / cancel_action)           = state<Terminating>,
            state<Answered>       +  event<AckReceived>                                                  = state<Confirmed>,
            state<Answered>       + (event<CancelReceived>                   / cancel_action)           = state<Terminating>,
            state<Answered>       + (event<CallDisconnected>                 / disconnect_action)       = state<Terminating>,
            state<Confirmed>      + (event<ByeReceived>                      / bye_action)              = state<Terminating>,
            state<Confirmed>      + (event<CallDisconnected>                 / disconnect_action)       = state<Terminating>,
            state<Confirmed>      + (event<CancelReceived>                   / cancel_action)           = state<Terminating>,
            state<Failed>         +  event<CallDisconnected>                                             = X,
            state<Terminating>                                                                          = X
        );
        // clang-format on
    }
};

} // namespace SIPI
