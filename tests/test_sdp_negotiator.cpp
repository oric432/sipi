#include <catch2/catch_test_macros.hpp>

#include <pjlib.h>

#include "SdpNegotiator.hpp"

static constexpr pj_size_t kPoolSize = 4096;

struct PjPool {
    pj_caching_pool cp_{};
    pj_pool_t* pool_;

    PjPool() {
        pj_init();
        pj_caching_pool_init(&cp_, &pj_pool_factory_default_policy, 0);
        pool_ = pj_pool_create(&cp_.factory, "test", kPoolSize, kPoolSize, nullptr); // NOLINT(cppcoreguidelines-prefer-member-initializer)
    }
    ~PjPool() {
        pj_pool_release(pool_);
        pj_caching_pool_destroy(&cp_);
    }

    PjPool(const PjPool&) = delete;
    PjPool& operator=(const PjPool&) = delete;
    PjPool(PjPool&&) = delete;
    PjPool& operator=(PjPool&&) = delete;
};

TEST_CASE_METHOD(PjPool, "parse_remote: SDP with no audio section returns error", "[sdp]") {
    SdpNegotiator neg{pool_};

    constexpr std::string_view kSdp = "v=0\r\n"
                                      "o=- 0 0 IN IP4 192.168.1.100\r\n"
                                      "s=-\r\n"
                                      "c=IN IP4 192.168.1.100\r\n"
                                      "t=0 0\r\n"
                                      "m=video 5004 RTP/AVP 96\r\n";

    REQUIRE_FALSE(neg.parse_remote(kSdp).has_value());
}

TEST_CASE_METHOD(PjPool, "parse_remote: audio with only PCMU returns error", "[sdp]") {
    SdpNegotiator neg{pool_};

    constexpr std::string_view kSdp = "v=0\r\n"
                                      "o=- 0 0 IN IP4 192.168.1.100\r\n"
                                      "s=-\r\n"
                                      "c=IN IP4 192.168.1.100\r\n"
                                      "t=0 0\r\n"
                                      "m=audio 4000 RTP/AVP 0\r\n"
                                      "a=rtpmap:0 PCMU/8000\r\n";

    REQUIRE_FALSE(neg.parse_remote(kSdp).has_value());
}

TEST_CASE_METHOD(PjPool, "build_local: produces SDP with PCMA at given IP and port", "[sdp]") {
    const std::string result =
        SdpNegotiator::build_local("10.0.0.1", 40000); // NOLINT(cppcoreguidelines-avoid-magic-numbers)
    REQUIRE_FALSE(result.empty());
    CHECK(result.find("m=audio 40000") != std::string::npos);
    CHECK(result.find("RTP/AVP 8") != std::string::npos);
    CHECK(result.find("10.0.0.1") != std::string::npos);
}

TEST_CASE_METHOD(PjPool, "parse_remote: valid PCMA offer returns remote IP and port", "[sdp]") {
    SdpNegotiator neg{pool_};

    constexpr std::string_view kSdp = "v=0\r\n"
                                      "o=- 0 0 IN IP4 192.168.1.100\r\n"
                                      "s=-\r\n"
                                      "c=IN IP4 192.168.1.100\r\n"
                                      "t=0 0\r\n"
                                      "m=audio 4000 RTP/AVP 8\r\n"
                                      "a=rtpmap:8 PCMA/8000\r\n";

    auto result = neg.parse_remote(kSdp);
    REQUIRE(result.has_value());
    CHECK(result->ip_ == "192.168.1.100");
    CHECK(result->port_ == 4000); // NOLINT(cppcoreguidelines-avoid-magic-numbers)
}
