#pragma once

#include <string>
#include <string_view>

#include <pj/pool.h>

#include "utils/error.hpp"

struct RemoteSdp {
    std::string ip_;
    uint16_t port_{};
};

class SdpNegotiator {
public:
    explicit SdpNegotiator(pj_pool_t* pool);

    Error::Result<RemoteSdp> parse_remote(std::string_view sdp);
    static std::string build_local(std::string_view local_ip, uint16_t local_port);

private:
    pj_pool_t* pool_;
};
