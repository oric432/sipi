#include "Settings.hpp"
#include "utils/log.hpp"

using namespace SIPI;

int main() {
    Log::init_logging();

    auto res = Settings::load("config.toml");
    if (!res) {
        Log::crash_error(std::format("Failed to load settings: {}", res.error().what()));
    }

    Log::app()->info("\nSettings loaded successfully:\n{}", res->dump());
}