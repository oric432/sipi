include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

FetchContent_Declare(
    boost_sml
    URL https://github.com/boost-ext/sml/archive/refs/tags/v1.1.11.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP ON
)
FetchContent_MakeAvailable(boost_sml)

add_library(boost_sml INTERFACE)
target_include_directories(boost_sml SYSTEM INTERFACE
    "${boost_sml_SOURCE_DIR}/include"
)
