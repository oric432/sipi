include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

set(BOOST_INCLUDE_LIBRARIES asio system CACHE STRING "" FORCE)
set(BOOST_ENABLE_CMAKE ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

fetchcontent_declare(
  Boost
  URL https://github.com/boostorg/boost/releases/download/boost-1.87.0/boost-1.87.0-cmake.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)
FetchContent_MakeAvailable(Boost)
