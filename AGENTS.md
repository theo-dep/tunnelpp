# AGENTS.md

tunnelpp: a C++23 HTTP tunnel. `common/` is a static library shared by the server (`server/`, deployed with
Docker) and the client (`client/`, a CLI also distributed as the npm package `@tunnelpp/client`).

## Hierarchy

This file applies to the whole repository. A directory may have its own `AGENTS.md` with more specific rules;
on conflict, the `AGENTS.md` closest to the file wins. `docs/` pages are reference documentation (the why), not
instructions.

| Directory | Content                                        | Instructions     |
| --------- | ---------------------------------------------- | ---------------- |
| `common/` | Static library shared by server and client     | `common/AGENTS.md` |
| `server/` | Tunnel server (binary + Docker image)          | `server/AGENTS.md` |
| `client/` | Tunnel client CLI + npm wrapper                | `client/AGENTS.md` |
| `tests/`  | End-to-end tests (server + client + local echo) | this file        |
| `docs/`   | Documentation (MkDocs, GitHub Pages)           | this file        |
| `.github/` | CI/CD workflows, actions, Dependabot          | `.github/AGENTS.md` |

## Rules

- Run every command from the repository root (CMake presets, `Justfile` and configs live there).
- Tests first: write or update the test before the code, check it fails, then implement. Work call-site first,
  then the implementation that fits the best call site.
- Keep the code as simple as possible: the less code and the fewer comments, the better. No speculative
  features.
- Any change of behaviour comes with a test.
- Tests run in Debug with AddressSanitizer and UBSan (`just test`).
- Never edit generated or vendored content: `build/`, `vcpkg_installed/`, `dist/`, `node_modules/`.
- Do not add dependencies (`vcpkg.json`, `client/npm/package.json`) without asking.
- Format only the files you changed (`just format <files>`), never the whole repository.
- To lint C++, run `clang-tidy` directly with a compile-commands database, on the changed files only:
  `just tidy <files>` (uses `build/clang-lint`). Never run the full lint build locally, it is very slow.
- CI (`.github/workflows/`) runs the same builds, tests and lints: keep it green. See `.github/AGENTS.md`
  for the full pipeline documentation.
- A change of behaviour described in `docs/` updates the page in the same change.

## Code style

- `.h` / `.cpp` files, flat in each library directory. An implementation stays in a `.h` only when it fits on
  one line, anything longer goes in the matching `.cpp` (templates excepted).
- Compiled with `-Wall -Wextra -Werror -pedantic`: no warning is acceptable.
- clang-format (`.clang-format`): 4 spaces, braces on their own line for functions/classes/namespaces,
  namespace content indented.
- clang-tidy naming (`.clang-tidy`): `CamelCase` classes and enums, `lower_case` functions, variables and
  members, private/protected members prefixed with `_`, `UPPER_CASE` macros.
- Forward-declare as much as possible in headers; hold `httplib` objects behind pointers.
- Includes of headers of the same library use `""`, every other include (another project library, system,
  third-party) uses `<>`; the `""` block and the `<>` blocks are separated by a blank line.
- Methods are `const` by default: a non-const method is the exception and needs a reason.
- Log through `spdlog` (initialized by `logging::init`), not `std::cout`.
- Prefer modern C++ (`std::optional`, ranges, `std::chrono`) as in the surrounding code.
- Test cases are named after the tested function: `TEST_CASE("base64_encode")`.

## Coverage

- `just coverage` builds the tests with clang source-based coverage (`-fprofile-instr-generate
  -fcoverage-mapping`), runs them and prints an `llvm-cov` report over `common/`, `server/`, `client/` and
  `tests/` (vcpkg and system headers excluded).
- Coverage has no blocking threshold: raising it is the work, not a gate. CI publishes the report as an
  artifact (`.github/workflows/coverage.yml`).

## Git

- Do not commit, push or create branches unless asked.
- Commit messages: conventional commits (`type(scope): description`), English, imperative mood, lowercase, no trailing period, one short line.
  - `feat`: new feature → minor version bump
  - `fix`: bug fix → patch version bump
  - `perf`: performance improvement → patch version bump
  - `chore`, `docs`, `ci`, `refactor`, `style`, `test`, `build`, `revert`: no version bump
- Releases are automated: push to `main` triggers `python-semantic-release` (see `.github/AGENTS.md`). Never create version tags manually.
