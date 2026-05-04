add_executable(sipi_tests
    test_sdp_negotiator.cpp
    test_rtp_session.cpp
    test_call_state_machine.cpp
    ${CMAKE_SOURCE_DIR}/src/SdpNegotiator.cpp
    ${CMAKE_SOURCE_DIR}/src/RtpSession.cpp
)

target_include_directories(sipi_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(sipi_tests PRIVATE
    Catch2::Catch2WithMain
    pjsip_all
    Boost::asio
    Boost::system
    boost_sml
    project_warnings
    project_options
)

include(Catch)
catch_discover_tests(sipi_tests)
