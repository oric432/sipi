#pragma once

#include <optional>
#include <string_view>

#include "Events.hpp"

namespace SIPI {

struct ICallContext {
    ICallContext()                             = default;
    virtual ~ICallContext()                    = default;
    ICallContext(const ICallContext&)          = delete;
    ICallContext& operator=(const ICallContext&) = delete;
    ICallContext(ICallContext&&)               = delete;
    ICallContext& operator=(ICallContext&&)    = delete;

    virtual void                     send_trying()                         = 0;
    virtual std::optional<SdpParsed> parse_sdp(std::string_view sdp_body) = 0;
    virtual bool                     open_rtp()                            = 0;
    virtual void                     send_ringing()                        = 0;
    virtual void                     send_ok()                             = 0;
    virtual void                     send_reject(int code)                 = 0;
    virtual void                     close_rtp()                           = 0;
    virtual void                     send_bye_ok()                         = 0;
};

} // namespace SIPI
