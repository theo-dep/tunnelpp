set(target_name server_tests)
add_executable(${target_name}
    test_pending.cpp
    test_router.cpp
    test_session.cpp
)

target_link_libraries(${target_name} PRIVATE
    server_core
    Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(${target_name})
