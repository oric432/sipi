add_executable(sipi_tests
    test_sdp_negotiator.cpp
    ${CMAKE_SOURCE_DIR}/src/SdpNegotiator.cpp
)

target_include_directories(sipi_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(sipi_tests PRIVATE
    Catch2::Catch2WithMain
    pjsip_all
    project_warnings
    project_options
)

include(Catch)
catch_discover_tests(sipi_tests)
