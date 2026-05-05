#include "Settings.hpp"
#include "SipEndpoint.hpp"
#include "utils/log.hpp"

using namespace SIPI;

int main() {
    Log::init_logging();

    auto res = Settings::load("config.toml");
    if (!res) {
        Log::crash_error(std::format("Failed to load settings: {}", res.error().what()));
    }

    Log::app()->info("Loaded settings:\n{}\n", res->dump());

    Log::set_log_level(res->get<std::string>(Settings::Path::kLOG_LEVEL));

    SipEndpoint endpoint{*res};
    endpoint.run();
}
