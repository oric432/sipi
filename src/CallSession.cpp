#include "CallSession.hpp"

#include "utils/log.hpp"

namespace SIPI {

CallSession::CallSession(const InviteReceived& event)
    : call_id_(event.rdata_->msg_info.cid->id.ptr,
               static_cast<std::size_t>(event.rdata_->msg_info.cid->id.slen))
    , inv_(event.inv_)
{
    static constexpr int kStatusTrying = 100;
    pjsip_tx_data* tdata = nullptr;
    pj_status_t    st    = pjsip_inv_initial_answer(inv_, event.rdata_, kStatusTrying, nullptr, nullptr, &tdata);
    if (st != PJ_SUCCESS) {
        Log::app()->warn("[{}] 100 Trying: create failed", call_id_);
        return;
    }
    st = pjsip_inv_send_msg(inv_, tdata);
    if (st != PJ_SUCCESS) {
        Log::app()->warn("[{}] 100 Trying: send failed", call_id_);
    } else {
        Log::app()->info("[{}] 100 Trying sent", call_id_);
    }
}

} // namespace SIPI
