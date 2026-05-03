#pragma once

#include <pjsip_ua.h>

struct InviteReceived {
    pjsip_inv_session* inv_;
    pjsip_rx_data*     rdata_;
};
