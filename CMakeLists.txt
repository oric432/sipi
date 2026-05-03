cmake_minimum_required(VERSION 3.23)
project(sipi LANGUAGES C CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(PreventInSourceBuilds)
include(StandardProjectSettings)

add_library(project_warnings INTERFACE)
add_library(project_options INTERFACE)

include(CompilerWarnings)
myproject_set_project_warnings(project_warnings FALSE "" "" "" "")

include(Sanitizers)
myproject_enable_sanitizers(project_options FALSE FALSE FALSE FALSE FALSE)

include(${CMAKE_SOURCE_DIR}/third_party/tomlplusplus.cmake)
include(${CMAKE_SOURCE_DIR}/third_party/spdlog.cmake)
include(${CMAKE_SOURCE_DIR}/third_party/boost.cmake)
include(${CMAKE_SOURCE_DIR}/third_party/boost-sml.cmake)
include(${CMAKE_SOURCE_DIR}/third_party/pjsip.cmake)
include(${CMAKE_SOURCE_DIR}/third_party/catch2.cmake)

add_executable(sipi
    src/main.cpp
    src/Settings.cpp
    src/SipModule.cpp
    src/SipEndpoint.cpp
)

target_precompile_headers(sipi PRIVATE
    src/pch.hpp
)

target_include_directories(sipi PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(sipi PRIVATE
    project_warnings
    project_options
    tomlplusplus::tomlplusplus
    spdlog::spdlog_header_only
    Boost::asio
    Boost::system
    boost_sml
    pjsip_all
)

include(ClangFormat)
add_clang_format_target(format ${CMAKE_SOURCE_DIR}/src)

add_subdirectory(tests)
