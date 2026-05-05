add_executable(sipi_tests
    test_sdp_negotiator.cpp
    test_rtp_session.cpp
    test_call_state_machine.cpp
    ${CMAKE_SOURCE_DIR}/src/sip/SdpNegotiator.cpp
    ${CMAKE_SOURCE_DIR}/src/rtp/RtpSession.cpp
)

target_include_directories(sipi_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/call
    ${CMAKE_SOURCE_DIR}/src/sip
    ${CMAKE_SOURCE_DIR}/src/rtp
)

target_link_libraries(sipi_tests PRIVATE
    Catch2::Catch2WithMain
    spdlog::spdlog_header_only
    pjsip_all
    Boost::asio
    Boost::system
    boost_sml
    project_warnings
    project_options
)

include(Catch)
catch_discover_tests(sipi_tests)
