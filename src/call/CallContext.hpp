#pragma once

#include <optional>
#include <string>

#include <boost/asio/io_context.hpp>
#include <pjsip.h>

#include "Events.hpp"
#include "RtpSession.hpp"
#include "SdpNegotiator.hpp"
#include "Settings.hpp"

namespace SIPI {

class CallContext {
public:
    explicit CallContext(const IncomingInvite& ev, boost::asio::io_context& ioc, const Settings& settings);

    // PJSIP dialog/inv session creation — called as SM action
    bool create_uas_invite_session();

    // Getter for session's inv pointer (null if setup failed)
    [[nodiscard]] pjsip_inv_session* inv() const { return inv_; }

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

    pjsip_rx_data* rdata_{};          // Initial request (kept for SDP and send_trying)
    pjsip_endpoint* endpt_{};         // PJSIP endpoint (for verify and dialog creation)
    int mod_id_{};                    // Module ID (stored for reference)
    pjsip_inv_session* inv_{};        // Null until create_uas_invite_session() succeeds

    std::optional<SdpNegotiator> negotiator_;  // Emplaced after dialog pool is available
    RtpSession rtp_;
    boost::asio::io_context& ioc_;

    std::string call_id_;
    std::string local_sdp_;
    uint16_t port_min_;
    uint16_t port_max_;
    std::string public_ip_;
};

} // namespace SIPI
