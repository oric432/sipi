#pragma once

// C++ Standard Library
#include <cstddef>     // IWYU pragma: export
#include <cstdlib>     // IWYU pragma: export
#include <expected>    // IWYU pragma: export
#include <format>      // IWYU pragma: export
#include <iostream>    // IWYU pragma: export
#include <sstream>     // IWYU pragma: export
#include <string>      // IWYU pragma: export
#include <string_view> // IWYU pragma: export
#include <vector>      // IWYU pragma: export

// Dependencies
#define TOML_EXCEPTIONS 0
#include <toml++/toml.hpp>

#include <boost/asio.hpp>
#include <boost/sml.hpp>
#include <spdlog/spdlog.h>
