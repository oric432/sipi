#pragma once

// toml++ config
#define TOML_EXCEPTIONS 0
#include <toml++/toml.hpp>

#include "utils/assert.hpp"
#include "utils/error.hpp"
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace SIPI {

class Settings {
public:
    // Schema and its defaults
    static constexpr std::string_view kDefaultsToml = R"toml(
        [sip]
        bind_address = "0.0.0.0"
        bind_port = 5060
        public_address = "192.168.1.50"   # used in SDP o= and c= lines

        [rtp]
        bind_address = "0.0.0.0"
        port_min = 40000
        port_max = 41000

        [logging]
        level = "info"
        pjsip_level = 0   # PJSIP log verbosity: 0=off, 1=fatal, 2=error, 3=warning, 4=info, 5=debug, 6=trace

    )toml";

    struct Path {
        static constexpr auto kSIP_BIND_ADDRESS = "sip.bind_address";
        static constexpr auto kSIP_BIND_PORT = "sip.bind_port";
        static constexpr auto kSIP_PUBLIC_ADDRESS = "sip.public_address";

        static constexpr auto kRTP_BIND_ADDRESS = "rtp.bind_address";
        static constexpr auto kRTP_PORT_MIN = "rtp.port_min";
        static constexpr auto kRTP_PORT_MAX = "rtp.port_max";

        static constexpr auto kLOG_LEVEL = "logging.level";
        static constexpr auto kLOG_PJSIP_LEVEL = "logging.pjsip_level";
    };

    static Error::Result<Settings> load(std::string_view file_path);

    template <typename T>
    Error::Result<T> try_get(std::string_view path) const {
        const toml::node_view<const toml::node> node = table_.at_path(path);
        auto value = node.value<T>();
        if (!value) {
            std::ostringstream oss;
            oss << "Missing or wrong type at '" << path << "'";
            return std::unexpected(Error::make_error().with_context(oss.str()));
        }
        return *value;
    }

    // Generic getter by TOML path (nested supported via at_path)
    template <typename T>
    T get(std::string_view path) const {
        auto res = try_get<T>(path);
        ASSERTM(res.has_value(), res ? "" : res.error().what());
        return *res;
    }

    [[nodiscard]] std::string dump() const;

private:
    explicit Settings(toml::table table)
        : table_(std::move(table)) {}

    static void merge_into(toml::table& dst, const toml::table& src);

    toml::table table_;
};

} // namespace SIPI
