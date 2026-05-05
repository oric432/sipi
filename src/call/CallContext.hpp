#pragma once

#include <string>

#include <boost/asio/io_context.hpp>

#include "Events.hpp"
#include "RtpSession.hpp"
#include "SdpNegotiator.hpp"
#include "Settings.hpp"

namespace SIPI {

class CallContext {
public:
    explicit CallContext(const InviteReceived& ev, boost::asio::io_context& ioc, const Settings& settings);

    void send_trying();
    std::optional<SdpParsed> parse_sdp();
    bool open_rtp();
    void send_ringing();
    void send_ok();
    void send_reject(int code);
    void close_rtp();
    void send_bye_ok();

private:
    std::string_view extract_sdp_body() const;

    pjsip_inv_session* inv_;
    pjsip_rx_data* initial_rdata_;
    std::string call_id_;

    SdpNegotiator negotiator_;
    RtpSession rtp_;
    boost::asio::io_context& ioc_;

    std::string local_sdp_;
    uint16_t port_min_;
    uint16_t port_max_;
    std::string public_ip_;
};

} // namespace SIPI
