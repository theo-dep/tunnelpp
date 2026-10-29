set(target_name common_tests)
add_executable(${target_name}
    test_base64.cpp
    test_config.cpp
    test_headers.cpp
    test_protocol.cpp
    test_stringutils.cpp
    test_subdomain.cpp
    test_url.cpp
    test_uuid.cpp
)

target_link_libraries(${target_name} PRIVATE
    ${common_lib_name}
    Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(${target_name})
