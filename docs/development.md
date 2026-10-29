# Development

## Prerequisites

- CMake >= 3.26, Ninja, clang (LLVM)
- a [vcpkg](https://github.com/microsoft/vcpkg) checkout, with the `VCPKG_ROOT` environment variable pointing
  to it
- [just](https://github.com/casey/just) (optional, for the shorthand commands)

Dependencies (cpp-httplib, nlohmann-json, spdlog, Catch2, argparse, OpenSSL) are managed by vcpkg in manifest
mode (`vcpkg.json`).

## Commands

| Goal | Command |
| ---- | ------- |
| Configure | `just configure` (`cmake --preset clang`) |
| Build (Debug / Release) | `just build-debug` / `just build-release` |
| Build and run all tests (Debug + sanitizers) | `just test` |
| Run tests filtered by name | `just test 'base64_encode*'` |
| Coverage report | `just coverage` |
| clang-tidy on changed files | `just tidy common/base64.cpp` |
| clang-format on changed files | `just format common/base64.cpp` |

Binaries are written to `build/<preset>/<Config>/` (multi-config generator).

## CMake presets

| Preset | Purpose |
| ------ | ------- |
| `clang` | base preset: clang + vcpkg toolchain (`$env{VCPKG_ROOT}`), Ninja Multi-Config |
| `clang-test` | `clang` + tests + AddressSanitizer/UBSan |
| `clang-lint` | `clang` + `CMAKE_CXX_CLANG_TIDY` (full clang-tidy build, used by CI) |
| `clang-debug` / `clang-release` / `clang-test-debug` | build presets (Debug / Release) |
| `clang-test-debug` | test preset: `ctest --preset clang-test-debug` |

The presets use the unversioned `clang`/`clang++` names (what CI runners provide). On a machine where
clang is versioned only (e.g. `clang-22` on Ubuntu), pass the compilers on the command line:

```
cmake --preset clang-test -DCMAKE_C_COMPILER=clang-22 -DCMAKE_CXX_COMPILER=clang++-22
```

## Layout

```
common/     static library shared by server and client (protocol, base64, url, config, ...)
server/     tunnelpp-server (library server_core + main.cpp) + Dockerfile
client/     tunnelpp-client (library client_core + main.cpp) + npm/ wrapper package
tests/      end-to-end tests (real server + real client + local echo server)
docs/       this documentation (MkDocs Material)
```

## Workflow (TDD)

1. Write the test first (`<lib>/tests/test_<module>.cpp`, Catch2 v3), watch it fail.
2. Implement the module (`<lib>/<module>.h` + `.cpp`), watch the test pass.
3. Keep the code as simple as possible; format and lint only the files you changed (`just format`,
   `just tidy`).
4. Run the whole suite (`just test`) before finishing: a change in `common/` can break the server and the
   client.

Test cases are named after the tested function (`TEST_CASE("base64_encode")`).

## Tests and sanitizers

Tests run in Debug with AddressSanitizer and UBSan (`TUNNELPP_ENABLE_SANITIZERS=ON`, set by the
`clang-test` preset). The end-to-end suite (`tests/e2e_test.cpp`) starts a real server, a real client and a
local echo server on ephemeral ports, and pushes real HTTP traffic through the tunnel.

## Coverage

`just coverage` builds the tests with clang source-based coverage (`-fprofile-instr-generate
-fcoverage-mapping`), runs them, and prints an `llvm-cov` report over `common/`, `server/`, `client/` and
`tests/` (vcpkg and system headers excluded). CI publishes the report as an artifact
(`.github/workflows/coverage.yml`). Coverage has no blocking threshold: raising it is the work, not a gate.

## Dependencies

Dependencies are pinned by `vcpkg.json` (manifest mode, versions from the vcpkg checkout). Dependabot
updates the GitHub Actions and the npm package (`.github/dependabot.yml`); it does not track vcpkg ports —
to update a dependency, update the vcpkg checkout (`git -C "$VCPKG_ROOT" pull`) and clear the vcpkg binary
cache, or pin a `builtin-baseline` in `vcpkg.json` and run `vcpkg x-update-baseline`.

Do not add dependencies without asking.

## Continuous integration

- `.github/workflows/ci.yml` — build + tests on Linux, Windows and macOS (clang), plus a clang-tidy build.
- `.github/workflows/coverage.yml` — coverage report artifact.
- `.github/workflows/release.yml` — prebuilt binaries per platform on tags `v*`.
- `.github/workflows/docker.yml` — server image to GHCR.
- `.github/workflows/docs.yml` — publishes this documentation to GitHub Pages.
