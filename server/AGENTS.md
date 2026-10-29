# AGENTS.md — server

C++23 tunnel server (`tunnelpp-server`, library `server_core`) built with CMake + vcpkg + clang
(cpp-httplib, nlohmann-json, spdlog, OpenSSL). It links `common/`, the shared library documented in
`common/AGENTS.md`.

This file inherits the root `AGENTS.md` rules (see Hierarchy there).

## Commands

| Goal                                | Command                                   |
| ----------------------------------- | ----------------------------------------- |
| Server tests                        | `just test '[test_*]'` (all tests)        |
| Debug build of the server           | `just build-debug` then `build/local/Debug/tunnelpp-server` |
| Run the dev server                  | `TUNNELPP_TOKEN=x build/local/Debug/tunnelpp-server` |
| clang-tidy on changed files         | `just tidy server/foo.cpp`                |
| Format changed files                | `just format server/foo.cpp server/foo.h` |

## Layout

- `tunnel_server.*`: `TunnelServer`: public listener (HTTP + WebSocket gateway `/tunnel`), admin listener,
  request routing, stats. Owns the `httplib::Server` instances behind `unique_ptr` (forward declared).
- `session.*`: `Session` (one connected tunnel) and `SessionManager` (subdomain registry).
- `pending.*`: `PendingRequestTable`: public requests waiting for the client response over WebSocket.
- `router.*`: `Host` header to subdomain extraction.
- `main.cpp`: configuration, logging, signal handling.
- `Dockerfile`, `docker-compose.yml` (at the repository root): container image.

## Tests (Catch2 v3)

- `server/tests/test_<module>.cpp`, one `TEST_CASE` per function. Add new files to
  `server/tests/CMakeLists.txt`.
- `tests/e2e_test.cpp` (repository root) exercises the real server with a real client over WebSocket.
- Any change of behaviour comes with a test, written first (root rule).

## Code style

Same rules as the root `AGENTS.md`.

## Do not

- Terminate TLS in the server: it sits behind a reverse proxy (see `docs/server.md` and `docs/security.md`).
- Expose the admin port publicly: bind it to localhost or an internal network, or set `TUNNELPP_ADMIN_TOKEN`.
