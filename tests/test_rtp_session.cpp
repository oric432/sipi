#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <boost/asio.hpp>

#include "RtpSession.hpp"
#include "rtp/RtpPacket.hpp"

static constexpr uint16_t kPortMin = 40000;
static constexpr uint16_t kPortMax = 41000;

namespace {

std::vector<uint8_t> make_rtp(uint16_t seq, uint32_t ts, std::size_t payload_bytes) {
    RtpCpp::RtpPacket<std::vector<uint8_t>> pkt;
    pkt.set_sequence_number(seq);
    pkt.set_timestamp(ts);
    pkt.set_payload_type(8);
    pkt.set_payload_size(payload_bytes);
    auto span = pkt.packet();
    return std::vector<uint8_t>{span.begin(), span.end()};
}

} // namespace

TEST_CASE("open: binds to a port in [port_min, port_max] and returns it", "[rtp]") {
    boost::asio::io_context ioc;
    RtpSession sess{"test-call"};

    auto result = sess.open(ioc, kPortMin, kPortMax);
    REQUIRE(result.has_value());
    CHECK(result.value() >= kPortMin);
    CHECK(result.value() <= kPortMax);
    CHECK(sess.port() == result.value());
}

TEST_CASE("recv: handler called for each received packet", "[rtp]") {
    using namespace boost::asio::ip;
    boost::asio::io_context ioc;

    int call_count = 0;
    RtpSession sess{"test-call", [&](const ReceivedRtp& /*r*/) { ++call_count; }};

    auto result = sess.open(ioc, kPortMin, kPortMax);
    REQUIRE(result.has_value());

    udp::socket sender{ioc, udp::v4()};
    udp::endpoint dest{address_v4::loopback(), result.value()};

    for (int i = 0; i < 3; ++i) {
        auto buf = make_rtp(static_cast<uint16_t>(i), static_cast<uint32_t>(i * 160), 160);
        sender.send_to(boost::asio::buffer(buf), dest);
    }

    ioc.run_for(std::chrono::milliseconds{200});

    CHECK(call_count == 3);
}
