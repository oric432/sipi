#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include <boost/asio.hpp>

#include "utils/error.hpp"

struct ReceivedRtp {
    std::string remote_ip_;
    uint16_t remote_port_{};
    uint16_t seq_{};
    uint32_t ts_{};
    std::size_t payload_size_{};
};

class RtpSession {
public:
    using PacketHandler = std::function<void(const ReceivedRtp&)>;

    explicit RtpSession(std::string call_id, PacketHandler on_packet = {});
    ~RtpSession();

    RtpSession(const RtpSession&) = delete;
    RtpSession& operator=(const RtpSession&) = delete;
    RtpSession(RtpSession&&) = delete;
    RtpSession& operator=(RtpSession&&) = delete;

    Error::Result<uint16_t> open(boost::asio::io_context& ioc, uint16_t port_min, uint16_t port_max);
    void close();
    [[nodiscard]] uint16_t port() const;

private:
    static constexpr std::size_t kRecvBufSize = 1500;

    void start_receive();

    std::string call_id_;
    PacketHandler on_packet_;
    uint16_t port_{};
    uint64_t total_packets_{};

    std::optional<boost::asio::ip::udp::socket> socket_;
    boost::asio::ip::udp::endpoint remote_ep_;
    std::array<uint8_t, kRecvBufSize> recv_buf_{};
};
