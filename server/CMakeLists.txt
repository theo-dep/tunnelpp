add_library(server_core STATIC
    pending.cpp
    router.cpp
    session.cpp
    tunnel_server.cpp
)
target_include_directories(server_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(server_core PUBLIC tunnelpp::common)

add_executable(tunnelpp-server main.cpp)
target_link_libraries(tunnelpp-server PRIVATE server_core)

if(TUNNELPP_BUILD_TESTS)
    add_subdirectory(tests)
endif()
