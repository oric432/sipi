#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <queue>
#include <string_view>

#include <boost/sml.hpp>

#include "CallStateMachine.hpp"

using namespace boost::sml;
using namespace SIPI;

namespace {

struct MockCallContext {
    std::optional<SdpParsed> sdp_result_{
        SdpParsed{.remote_ip_ = "1.2.3.4", .remote_port_ = 20000, .has_audio_ = true, .supports_pcma_ = true}};
    bool rtp_result_{true};

    int trying_count_{};
    int ringing_count_{};
    int ok_count_{};
    int reject_count_{};
    int last_reject_code_{};
    int close_count_{};
    int bye_ok_count_{};

    void send_trying() { ++trying_count_; }

    std::optional<SdpParsed> parse_sdp() { return sdp_result_; }

    bool open_rtp() { return rtp_result_; }
    void send_ringing() { ++ringing_count_; }
    void send_ok() { ++ok_count_; }

    void send_reject(int code) {
        ++reject_count_;
        last_reject_code_ = code;
    }

    void close_rtp() { ++close_count_; }
    void send_bye_ok() { ++bye_ok_count_; }
};

// Non-null but never-dereferenced pointer satisfies the is_valid_invite guard.
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,performance-no-int-to-ptr,cppcoreguidelines-avoid-non-const-global-variables)
pjsip_inv_session* const kFakeInv = reinterpret_cast<pjsip_inv_session*>(static_cast<uintptr_t>(1));

using SM = sm<CallStateMachine<MockCallContext>, process_queue<std::queue>>;

// Catch2's expression decomposer cannot handle template args (< >) inside macros.
// Use type aliases so that CHECK/REQUIRE see plain type names with no angle brackets.
using InIdle      = boost::sml::front::state<Idle>;
using InAnswered  = boost::sml::front::state<Answered>;
using InConfirmed = boost::sml::front::state<Confirmed>;
using InFailed    = boost::sml::front::state<Failed>;

constexpr uint16_t kRemotePort = 20000;

} // namespace

TEST_CASE("happy path: INVITE → Answered → Confirmed → BYE → X", "[csm]") {
    MockCallContext ctx;
    SM              smach{ctx};

    REQUIRE(smach.is(InIdle{}));

    smach.process_event(InviteReceived{.inv_ = kFakeInv, .rdata_ = nullptr});
    // invite_action → SdpParsed (via back::process) → open_rtp_action → RtpReady → answer_action
    CHECK(smach.is(InAnswered{}));
    CHECK(ctx.trying_count_ == 1);
    CHECK(ctx.ringing_count_ == 1);
    CHECK(ctx.ok_count_ == 1);

    smach.process_event(AckReceived{});
    CHECK(smach.is(InConfirmed{}));

    smach.process_event(ByeReceived{});
    CHECK(smach.is(X));
    CHECK(ctx.close_count_ == 1);
    CHECK(ctx.bye_ok_count_ == 1);
}

TEST_CASE("SDP parse failure → Failed with 488", "[csm]") {
    MockCallContext ctx;
    ctx.sdp_result_ = std::nullopt;
    SM            smach{ctx};

    smach.process_event(InviteReceived{.inv_ = kFakeInv, .rdata_ = nullptr});
    CHECK(smach.is(InFailed{}));
    CHECK(ctx.reject_count_ == 1);
    CHECK(ctx.last_reject_code_ == kSipNotAcceptableHere);
}

TEST_CASE("SDP with no audio → Failed with 488", "[csm]") {
    MockCallContext ctx;
    ctx.sdp_result_ = SdpParsed{
        .remote_ip_ = "1.2.3.4", .remote_port_ = kRemotePort, .has_audio_ = false, .supports_pcma_ = true};
    SM            smach{ctx};

    smach.process_event(InviteReceived{.inv_ = kFakeInv, .rdata_ = nullptr});
    CHECK(smach.is(InFailed{}));
    CHECK(ctx.last_reject_code_ == kSipNotAcceptableHere);
}

TEST_CASE("SDP with no PCMA → Failed with 488", "[csm]") {
    MockCallContext ctx;
    ctx.sdp_result_ = SdpParsed{
        .remote_ip_ = "1.2.3.4", .remote_port_ = kRemotePort, .has_audio_ = true, .supports_pcma_ = false};
    SM            smach{ctx};

    smach.process_event(InviteReceived{.inv_ = kFakeInv, .rdata_ = nullptr});
    CHECK(smach.is(InFailed{}));
    CHECK(ctx.last_reject_code_ == kSipNotAcceptableHere);
}

TEST_CASE("RTP open failure → Failed with 500", "[csm]") {
    MockCallContext ctx;
    ctx.rtp_result_ = false;
    SM            smach{ctx};

    smach.process_event(InviteReceived{.inv_ = kFakeInv, .rdata_ = nullptr});
    CHECK(smach.is(InFailed{}));
    CHECK(ctx.last_reject_code_ == kSipInternalError);
}

TEST_CASE("CANCEL in Answered → X", "[csm]") {
    MockCallContext ctx;
    SM              smach{ctx};

    smach.process_event(InviteReceived{.inv_ = kFakeInv, .rdata_ = nullptr});
    REQUIRE(smach.is(InAnswered{}));

    smach.process_event(CancelReceived{});
    CHECK(smach.is(X));
    CHECK(ctx.close_count_ == 1);
}

TEST_CASE("CANCEL in Confirmed → X", "[csm]") {
    MockCallContext ctx;
    SM              smach{ctx};

    smach.process_event(InviteReceived{.inv_ = kFakeInv, .rdata_ = nullptr});
    smach.process_event(AckReceived{});
    REQUIRE(smach.is(InConfirmed{}));

    smach.process_event(CancelReceived{});
    CHECK(smach.is(X));
    CHECK(ctx.close_count_ == 1);
}

TEST_CASE("null inv pointer stays in Idle", "[csm]") {
    MockCallContext ctx;
    SM              smach{ctx};

    smach.process_event(InviteReceived{.inv_ = nullptr, .rdata_ = nullptr});
    CHECK(smach.is(InIdle{}));
    CHECK(ctx.trying_count_ == 0);
}

// NOTE: CancelReceived transitions from IncomingInvite and Trying are not
// unit-testable with the synchronous process_queue: those states are
// transient and never externally observable. The transitions exist in the
// table for correctness when/if RTP dispatch becomes async.
