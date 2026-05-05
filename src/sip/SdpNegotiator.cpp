#include "SdpNegotiator.hpp"

#include <pj/string.h>
#include <pjmedia/sdp.h>

SdpNegotiator::SdpNegotiator(pj_pool_t* pool)
    : pool_{pool} {}

Error::Result<RemoteSdp> SdpNegotiator::parse_remote(std::string_view sdp) {
    // pjmedia_sdp_parse tokenises the buffer in-place — must be mutable.
    // Validates audio section exists and offers PCMA (payload 8).
    std::string mutable_sdp{sdp};
    pjmedia_sdp_session* session{};

    if (pjmedia_sdp_parse(pool_, mutable_sdp.data(), mutable_sdp.size(), &session) != PJ_SUCCESS) {
        return std::unexpected(Error::make_error().with_context("SDP parse failed"));
    }

    // Find the audio media section
    pjmedia_sdp_media* audio{};
    for (unsigned i = 0; i < session->media_count; ++i) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        if (pj_strcmp2(&session->media[i]->desc.media, "audio") == 0) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            audio = session->media[i];
            break;
        }
    }
    if (audio == nullptr) {
        return std::unexpected(Error::make_error().with_context("no audio section in SDP"));
    }

    // Check if PCMA (payload 8) is offered
    bool has_pcma = false;
    for (unsigned i = 0; i < audio->desc.fmt_count; ++i) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        if (pj_strcmp2(&audio->desc.fmt[i], "8") == 0) {
            has_pcma = true;
            break;
        }
    }
    if (!has_pcma) {
        return std::unexpected(Error::make_error().with_context("PCMA (payload 8) not offered"));
    }

    // Get connection address - prefer media specific connection, fallback to session connection
    const pjmedia_sdp_conn* conn = (audio->conn != nullptr) ? audio->conn : session->conn;
    if (conn == nullptr) {
        return std::unexpected(Error::make_error().with_context("no connection address in SDP"));
    }

    return RemoteSdp{
        .ip_ = std::string(conn->addr.ptr, static_cast<std::size_t>(conn->addr.slen)),
        .port_ = static_cast<uint16_t>(audio->desc.port),
    };
}

std::string SdpNegotiator::build_local(std::string_view local_ip, uint16_t local_port) {
    return std::format(
        "v=0\r\n"
        "o=- 0 0 IN IP4 {0}\r\n"
        "s=-\r\n"
        "c=IN IP4 {0}\r\n"
        "t=0 0\r\n"
        "m=audio {1} RTP/AVP 8\r\n"
        "a=rtpmap:8 PCMA/8000\r\n",
        local_ip,
        local_port);
}
