# AGENTS.md — common

Small C++23 static library (`common`) linked by the server (`server/`) and the client (`client/`). It only
depends on cpp-httplib, nlohmann-json, spdlog, OpenSSL and Threads: no knowledge of `server/` or `client/`.

This file inherits the root `AGENTS.md` rules (see Hierarchy there).

## Commands

| Goal                                    | Command                              |
| --------------------------------------- | ------------------------------------ |
| `common/` tests                         | `just test '[test file]*'`           |
| `common/` tests, filtered by name       | `just test 'base64_encode*'`         |
| Everything depending on `common/`       | `just check` (all tests)             |
| clang-tidy on changed files             | `just tidy common/foo.cpp`           |
| Format changed files                    | `just format common/foo.cpp common/foo.h` |

## Modules

| Files          | Role                                                            |
| -------------- | --------------------------------------------------------------- |
| `base64.*`     | base64 encode/decode (message bodies)                           |
| `config.*`     | `ServerConfig` and environment variable helpers                 |
| `headers.*`    | `filter_tunnel_headers`: strip hop-by-hop/framing headers       |
| `logging.*`    | `logging::init(debug)`: spdlog setup                            |
| `protocol.*`   | Tunnel WebSocket protocol: `Message`, `encode/decode_message`   |
| `stringutils.*`| `lower`                                                         |
| `subdomain.*`  | Subdomain validation and random generation                      |
| `url.*`        | `Url` and `parse_url`                                           |
| `uuid.*`       | `make_uuid`: random v4 request ids                             |

- `CMakeLists.txt` also defines compile definitions **public** to every target: `CPPHTTPLIB_SSL_ENABLED`,
  `CPPHTTPLIB_WEBSOCKET_MAX_PAYLOAD_LENGTH` (64 MB) and `TUNNELPP_VERSION`.

## Tests (Catch2 v3)

- `common/tests/test_<module>.cpp`, one `TEST_CASE` per function. Add new files to
  `common/tests/CMakeLists.txt`.
- Any change of behaviour comes with a test, written first (root rule).

## Code style

Same rules as the root `AGENTS.md`: `-Werror`, `.clang-format`, `.clang-tidy` naming (`lower_case` functions,
`_` prefix for private members), headers of the same library with `""`, every other include with `<>`,
forward declarations in headers whenever possible.

## Do not

- Make `common/` depend on `server/` or `client/`.
- Change the protocol message format (`protocol.*`) without updating `docs/protocol.md` and the tests.
