#include "RtpSession.hpp"

#include <span>

#include "rtp/RtpPacket.hpp"

RtpSession::RtpSession(std::string call_id, PacketHandler on_packet)
    : call_id_{std::move(call_id)}
    , on_packet_{std::move(on_packet)} {}

RtpSession::~RtpSession() {
    close();
}

Error::Result<uint16_t> RtpSession::open(boost::asio::io_context& ioc, uint16_t port_min, uint16_t port_max) {
    using namespace boost::asio::ip;
    auto sock = udp::socket{ioc};
    sock.open(udp::v4());

    for (uint16_t port = port_min; port <= port_max; ++port) {
        boost::system::error_code ec;
        sock.bind(udp::endpoint{udp::v4(), port}, ec);
        if (!ec) {
            port_ = port;
            socket_ = std::move(sock);
            start_receive();
            return port_;
        }
    }

    return std::unexpected(Error::make_error().with_context("no free port in range"));
}

void RtpSession::close() {
    if (socket_.has_value()) {
        boost::system::error_code ec;
        socket_->close(ec);
        socket_.reset();
    }
}

uint16_t RtpSession::port() const {
    return port_;
}

void RtpSession::start_receive() {
    socket_->async_receive_from(
        boost::asio::buffer(recv_buf_),
        remote_ep_,
        [this](boost::system::error_code ec, std::size_t bytes) {
            if (ec) {
                return;
            }

            std::span<uint8_t> view{recv_buf_.data(), bytes};
            RtpCpp::RtpPacket<std::span<uint8_t>> pkt{view};
            if (pkt.parse(bytes) == RtpCpp::Result::kSuccess && on_packet_) {
                const auto& hdr = pkt.get_header();
                on_packet_(ReceivedRtp{
                    .remote_ip_ = remote_ep_.address().to_string(),
                    .remote_port_ = remote_ep_.port(),
                    .seq_ = hdr.sequence_number_,
                    .ts_ = hdr.timestamp_,
                    .payload_size_ = static_cast<std::size_t>(pkt.get_payload_size()),
                });
            }
            ++total_packets_;
            start_receive();
        });
}
