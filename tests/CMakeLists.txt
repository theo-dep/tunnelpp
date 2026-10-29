set(target_name e2e_tests)
add_executable(${target_name}
    e2e_test.cpp
)

target_link_libraries(${target_name} PRIVATE
    server_core
    client_core
    Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(${target_name})
