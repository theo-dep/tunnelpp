add_library(client_core STATIC
    args.cpp
    local_forwarder.cpp
    tunnel_client.cpp
)
target_include_directories(client_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(client_core PUBLIC tunnelpp::common)

add_executable(tunnelpp-client main.cpp)
target_link_libraries(tunnelpp-client PRIVATE client_core)

if(TUNNELPP_BUILD_TESTS)
    add_subdirectory(tests)
endif()
