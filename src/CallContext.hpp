#pragma once

#include <string>

#include <boost/asio/io_context.hpp>

#include "ICallContext.hpp"
#include "RtpSession.hpp"
#include "SdpNegotiator.hpp"
#include "Settings.hpp"

namespace SIPI {

class CallContext : public ICallContext {
public:
    explicit CallContext(const InviteReceived& ev, boost::asio::io_context& ioc, const Settings& settings);

    void send_trying() override;
    std::optional<SdpParsed> parse_sdp(std::string_view sdp_body) override;
    bool open_rtp() override;
    void send_ringing() override;
    void send_ok() override;
    void send_reject(int code) override;
    void close_rtp() override;
    void send_bye_ok() override;

private:
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
