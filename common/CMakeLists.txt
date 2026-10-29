set(common_lib_name common)

add_library(${common_lib_name} STATIC
    base64.cpp
    config.cpp
    headers.cpp
    logging.cpp
    protocol.cpp
    stringutils.cpp
    subdomain.cpp
    url.cpp
    uuid.cpp
)
add_library(tunnelpp::common ALIAS ${common_lib_name})

target_include_directories(${common_lib_name} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${common_lib_name} PUBLIC
    httplib::httplib
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
    spdlog::spdlog
    Threads::Threads
)
target_compile_definitions(${common_lib_name} PUBLIC
    CPPHTTPLIB_WEBSOCKET_MAX_PAYLOAD_LENGTH=67108864
    TUNNELPP_VERSION="${PROJECT_VERSION}"
)

if(TUNNELPP_BUILD_TESTS)
    add_subdirectory(tests)
endif()
