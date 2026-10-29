set(target_name client_tests)
add_executable(${target_name}
    test_args.cpp
    test_local_forwarder.cpp
)

target_link_libraries(${target_name} PRIVATE
    client_core
    Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(${target_name})
