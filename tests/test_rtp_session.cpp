#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <functional>
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

TEST_CASE("recv: echoes valid RTP back to sender", "[rtp]") {
    using namespace boost::asio::ip;
    boost::asio::io_context ioc;

    RtpSession sess{"test-call"};

    auto result = sess.open(ioc, kPortMin, kPortMax);
    REQUIRE(result.has_value());

    udp::socket sender{ioc, udp::v4()};
    udp::endpoint dest{address_v4::loopback(), result.value()};
    udp::endpoint echoed_from;
    std::array<uint8_t, 1500> echoed_buf{};
    boost::system::error_code receive_ec;
    std::size_t echoed_bytes = 0;

    auto buf = make_rtp(/*seq=*/42, /*ts=*/8000, /*payload_bytes=*/100);
    sender.async_receive_from(
        boost::asio::buffer(echoed_buf),
        echoed_from,
        [&](boost::system::error_code ec, std::size_t bytes) {
            receive_ec = ec;
            echoed_bytes = bytes;
        });
    sender.send_to(boost::asio::buffer(buf), dest);

    ioc.run_for(std::chrono::milliseconds{200});

    REQUIRE_FALSE(receive_ec);
    REQUIRE(echoed_bytes == buf.size());
    CHECK(echoed_from.port() == result.value());
    CHECK(std::equal(buf.begin(), buf.end(), echoed_buf.begin()));
}

TEST_CASE("recv: echoes each valid RTP packet", "[rtp]") {
    using namespace boost::asio::ip;
    boost::asio::io_context ioc;

    RtpSession sess{"test-call"};

    auto result = sess.open(ioc, kPortMin, kPortMax);
    REQUIRE(result.has_value());

    udp::socket sender{ioc, udp::v4()};
    udp::endpoint dest{address_v4::loopback(), result.value()};
    udp::endpoint echoed_from;
    std::array<uint8_t, 1500> echoed_buf{};
    int echo_count = 0;

    std::function<void()> receive_echo;
    receive_echo = [&] {
        sender.async_receive_from(
            boost::asio::buffer(echoed_buf),
            echoed_from,
            [&](boost::system::error_code ec, std::size_t /*bytes*/) {
                if (!ec) {
                    ++echo_count;
                    if (echo_count < 3) {
                        receive_echo();
                    }
                }
            });
    };
    receive_echo();

    for (int i = 0; i < 3; ++i) {
        auto buf = make_rtp(static_cast<uint16_t>(i), static_cast<uint32_t>(i * 160), 160);
        sender.send_to(boost::asio::buffer(buf), dest);
    }

    ioc.run_for(std::chrono::milliseconds{200});

    CHECK(echo_count == 3);
}

TEST_CASE("close: no echo after session is destroyed", "[rtp]") {
    using namespace boost::asio::ip;
    boost::asio::io_context ioc;

    int echo_count = 0;
    udp::socket sender{ioc, udp::v4()};
    udp::endpoint echoed_from;
    std::array<uint8_t, 1500> echoed_buf{};

    std::function<void()> receive_echo;
    receive_echo = [&] {
        sender.async_receive_from(
            boost::asio::buffer(echoed_buf),
            echoed_from,
            [&](boost::system::error_code ec, std::size_t /*bytes*/) {
                if (!ec) {
                    ++echo_count;
                    receive_echo();
                }
            });
    };
    receive_echo();

    {
        RtpSession sess{"test-call"};
        auto result = sess.open(ioc, kPortMin, kPortMax);
        REQUIRE(result.has_value());

        udp::endpoint dest{address_v4::loopback(), result.value()};

        sender.send_to(boost::asio::buffer(make_rtp(1, 160, 160)), dest);
        ioc.run_for(std::chrono::milliseconds{100});
        REQUIRE(echo_count == 1);

        // Send a second packet while the session is still alive, then let it go
        // out of scope — close() via destructor must cancel the pending recv
        sender.send_to(boost::asio::buffer(make_rtp(2, 320, 160)), dest);
    }  // ~RtpSession calls close()

    // Process the cancelled operation; handler must NOT fire
    ioc.restart();
    ioc.run_for(std::chrono::milliseconds{50});

    CHECK(echo_count == 1);
}
