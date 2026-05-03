#include "RtpSession.hpp"

RtpSession::RtpSession(std::string call_id, PacketHandler on_packet)
    : call_id_{std::move(call_id)}, on_packet_{std::move(on_packet)} {}

RtpSession::~RtpSession() { close(); }

Error::Result<uint16_t> RtpSession::open(boost::asio::io_context& ioc,
                                          uint16_t port_min, uint16_t port_max) {
    using namespace boost::asio::ip;
    auto sock = udp::socket{ioc};
    sock.open(udp::v4());

    for (uint16_t port = port_min; port <= port_max; ++port) {
        boost::system::error_code ec;
        sock.bind(udp::endpoint{udp::v4(), port}, ec);
        if (!ec) {
            port_   = port;
            socket_ = std::move(sock);
            start_receive();
            return port_;
        }
    }

    return std::unexpected(Error::make_error().with_context("no free port in range"));
}

void RtpSession::close() {}

uint16_t RtpSession::port() const { return port_; }

void RtpSession::start_receive() {}
