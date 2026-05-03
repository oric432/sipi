#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <boost/asio.hpp>

#include "RtpSession.hpp"
#include "rtp/RtpPacket.hpp"

static constexpr uint16_t kPortMin = 40000;
static constexpr uint16_t kPortMax = 41000;

namespace {

constexpr uint8_t kPcmaPayloadType = 8;

std::vector<uint8_t> make_rtp(uint16_t seq, uint32_t timestamp, std::size_t payload_bytes) {
    RtpCpp::RtpPacket<std::vector<uint8_t>> pkt;
    pkt.set_sequence_number(seq);
    pkt.set_timestamp(timestamp);
    pkt.set_payload_type(kPcmaPayloadType);
    (void)pkt.set_payload_size(payload_bytes);
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

TEST_CASE("recv: handler receives correct seq, ts, and payload_size", "[rtp]") {
    using namespace boost::asio::ip;
    boost::asio::io_context ioc;

    ReceivedRtp captured{};
    RtpSession sess{"test-call", [&](const ReceivedRtp& r) { captured = r; }};

    auto result = sess.open(ioc, kPortMin, kPortMax);
    REQUIRE(result.has_value());

    udp::socket sender{ioc, udp::v4()};
    udp::endpoint dest{address_v4::loopback(), result.value()};

    auto buf = make_rtp(/*seq=*/42, /*ts=*/8000, /*payload_bytes=*/100);
    sender.send_to(boost::asio::buffer(buf), dest);

    ioc.run_for(std::chrono::milliseconds{200});

    CHECK(captured.seq_ == 42);
    CHECK(captured.ts_ == 8000);
    CHECK(captured.payload_size_ == 100);
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

TEST_CASE("close: handler not called after session is destroyed", "[rtp]") {
    using namespace boost::asio::ip;
    boost::asio::io_context ioc;

    int call_count = 0;

    {
        RtpSession sess{"test-call", [&](const ReceivedRtp& /*r*/) { ++call_count; }};
        auto result = sess.open(ioc, kPortMin, kPortMax);
        REQUIRE(result.has_value());

        udp::socket sender{ioc, udp::v4()};
        udp::endpoint dest{address_v4::loopback(), result.value()};

        sender.send_to(boost::asio::buffer(make_rtp(1, 160, 160)), dest);
        ioc.run_for(std::chrono::milliseconds{100});
        REQUIRE(call_count == 1);

        // Send a second packet while the session is still alive, then let it go
        // out of scope — close() via destructor must cancel the pending recv
        sender.send_to(boost::asio::buffer(make_rtp(2, 320, 160)), dest);
    }  // ~RtpSession calls close()

    // Process the cancelled operation; handler must NOT fire
    ioc.run_for(std::chrono::milliseconds{50});

    CHECK(call_count == 1);
}
