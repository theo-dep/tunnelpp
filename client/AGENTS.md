# AGENTS.md — client

C++23 tunnel client (`tunnelpp-client`, library `client_core`) built with CMake + vcpkg + clang
(cpp-httplib, argparse, nlohmann-json, spdlog, OpenSSL). It links `common/`, the shared library documented
in `common/AGENTS.md`. The `npm/` directory is the `@tunnelpp/client` package: a wrapper that downloads the
prebuilt binary at install time and exposes a small JS API.

This file inherits the root `AGENTS.md` rules (see Hierarchy there).

## Commands

| Goal                                | Command                                   |
| ----------------------------------- | ----------------------------------------- |
| Client tests                        | `just test '[test_*]'` (all tests)        |
| Debug build of the client           | `just build-debug` then `build/local/Debug/tunnelpp-client` |
| clang-tidy on changed files         | `just tidy client/foo.cpp`                |
| Format changed files                | `just format client/foo.cpp client/foo.h` |

## Layout

- `args.*`: CLI definition (argparse) and `parse_options` validation.
- `tunnel_client.*`: `TunnelClient`: WebSocket connection, registration, reconnection with exponential
  backoff, request dispatch to worker threads.
- `local_forwarder.*`: `forward_to_local`: forwards one tunnel request to the local HTTP service.
- `main.cpp`: CLI entry point, signal handling.
- `npm/`: the `@tunnelpp/client` package (`package.json`, `index.js`, `bin/`, `scripts/install.js`).

## Tests (Catch2 v3)

- `client/tests/test_<module>.cpp`, one `TEST_CASE` per function. Add new files to
  `client/tests/CMakeLists.txt`.
- `tests/e2e_test.cpp` (repository root) exercises the real client with a real server over WebSocket.
- Any change of behaviour comes with a test, written first (root rule).

## Code style

Same rules as the root `AGENTS.md`.

## Do not

- Change the npm package name (`@tunnelpp/client`) or its binary download URL scheme without updating
  `docs/client.md` and the release workflow.
