# AGENTS.md — CI/CD (GitHub Actions)

This file documents the CI/CD pipeline. It applies to everything under `.github/`:
workflows, composite actions, Dependabot configuration.

## Overview

| Workflow | File | Trigger | Purpose |
|---|---|---|---|
| CI | `workflows/ci.yml` | push to `main`, PR | Multi-OS build + tests, clang-tidy lint |
| Coverage | `workflows/coverage.yml` | push to `main`, manual | LLVM coverage report (artifact) |
| Docker | `workflows/docker.yml` | push to `main` (path-filtered), published release | Build and push `ghcr.io/tunnelpp/server` image |
| Docs | `workflows/docs.yml` | push to `main` (path-filtered), manual | Build MkDocs and deploy GitHub Pages |
| Release | `workflows/release.yml` | push to `main` | Semantic-release (conventional commits) + multi-platform binaries |

## Workflows in detail

### CI (`ci.yml`)

Two jobs:

- **build-test** — 3-OS matrix (`ubuntu-latest`, `windows-latest`, `macos-latest`)
  - Installs Ninja + Clang (Linux via apt, Windows via `C:\Program Files\LLVM\bin`, macOS preinstalled)
  - Configures with the `clang-test` preset (tests + ASan/UBSan sanitizers)
  - Builds in Debug, runs `ctest`
  - `fail-fast: false`: a failure on one OS does not cancel the others

- **lint** — `ubuntu-latest` only
  - Configures with the `clang-lint` preset (`CMAKE_CXX_CLANG_TIDY`)
  - Builds with `--parallel 1` so clang-tidy runs on every file

### Coverage (`coverage.yml`)

- Linux only, manual or push to `main`
- Configures with `clang-test` + `-DTUNNELPP_BUILD_COVERAGE=ON` in `build/coverage`
- Runs tests with `LLVM_PROFILE_FILE`, merges with `llvm-profdata`
- Generates a text report (`llvm-cov report`) and HTML (`llvm-cov show`)
- Excludes `vcpkg_installed/`, `/usr/`, `build/` from the report
- Uploads the `coverage-report` artifact (HTML + profdata)

### Docker (`docker.yml`)

- Triggered on push to `main` when `server/**`, `common/**`, `vcpkg.json`, `CMakeLists.txt`, `CMakePresets.json` change
- Also triggered on published `release`
- QEMU + Buildx for multi-arch
- Tags: semver (`v1.2.3` → `1.2.3`, `1.2`), short SHA
- GHA cache (`cache-from`/`cache-to` type=gha, mode=max)
- Pushes to `ghcr.io/tunnelpp/server`

### Docs (`docs.yml`)

- Triggered on push to `main` when `docs/**` or `mkdocs.yml` change, or manually
- Builds MkDocs with `mkdocs-material`, `--strict` mode (fails on warnings)
- Deploys to GitHub Pages via `actions/deploy-pages`
- Concurrency `pages` with `cancel-in-progress: true`

### Release (`release.yml`)

Two phases:

- **semantic-release** — runs on every push to `main`
  - Installs `python-semantic-release` and analyzes commits since the last tag
  - If `feat` or `fix` commits are found: bumps version in `pyproject.toml` and `CMakeLists.txt`, updates `CHANGELOG.md`, commits with `[skip ci]`, creates tag `v{version}`, pushes, and creates a GitHub release with the changelog
  - Outputs `released` (true/false) and `version` for downstream jobs
  - Version rules: `feat` → minor bump (0.x.y → 0.x+1.0), `fix`/`perf` → patch bump (0.x.y → 0.x.y+1). `chore`, `docs`, `ci`, etc. do not trigger a release.

- **build** — 5-target matrix, only runs when `released == 'true'`
  - Checks out the release tag `v{version}`
  - Release build with the `clang` preset (no sanitizers)
  - Packages as `.zip` (Windows) or `.tar.gz` (others)
  - Uploads binaries to the GitHub release created by semantic-release

## Semantic release configuration

`pyproject.toml` — `[tool.semantic_release]`:

- **Version source**: `pyproject.toml:project.version` (canonical). `build_command` syncs the version to 3 other files:
  - `CMakeLists.txt` — `project(tunnelpp VERSION ...)`
  - `vcpkg.json` — `"version-string"`
  - `client/npm/package.json` — `"version"`
- **Tag format**: `v{version}` (e.g. `v0.2.0`)
- **Changelog**: auto-generated in `CHANGELOG.md`, release commits excluded
- **Commit parser**: conventional commits. `minor_tags = ["feat"]`, `patch_tags = ["fix", "perf"]`
- **Zero version**: `allow_zero_version = true`, `major_on_zero = false` (0.x → minor bump on `feat`)
- **Release commit message**: `chore(release): {version} [skip ci]` — the `[skip ci]` prevents re-triggering all workflows on the version-bump commit

## Dependabot

`.github/dependabot.yml` — weekly automatic updates with conventional commit messages:

| Ecosystem | Directory | Schedule | Commit prefix | Release? |
|---|---|---|---|---|
| `github-actions` | `/` | weekly | `chore(deps)` | no |
| `npm` | `/client/npm` | weekly | `chore(deps)` | no |
| `vcpkg` | `/` | weekly | `fix(deps)` | **yes** (patch bump) |
| `docker` | `/server` | weekly | `chore(deps)` | no |

vcpkg uses `fix(deps)` because C++ dependency updates often include security fixes (OpenSSL, httplib, etc.) that warrant a patch release. All other ecosystems use `chore(deps)` and never trigger a version bump. Example messages:
- `fix(deps): bump openssl from 3.3.0 to 3.3.1`
- `chore(deps): bump actions/checkout from 4 to 5`

## Composite action: `setup-vcpkg`

`.github/actions/setup-vcpkg/action.yml` — used by `ci.yml`, `coverage.yml` and `release.yml`.

- Checks out `microsoft/vcpkg` into `.vcpkg/`
- Bootstraps vcpkg (`.sh` on Linux/macOS, `.bat` on Windows)
- Installs `mono` on Linux/macOS (needed to run `nuget.exe`)
- Configures the vcpkg binary cache on a NuGet feed hosted on GitHub Packages
- Uses `secrets.GITHUB_TOKEN` for authentication
- Environment variables expected by the action: `VCPKG_ROOT`, `VCPKG_EXE`, `VCPKG_NUGET_FEED_URL`, `VCPKG_BINARY_SOURCES` (set at the `env:` level in each workflow)

## Permissions

All workflows follow the principle of least privilege:

- `ci.yml`, `coverage.yml`: `contents: read` + `packages: write` (for the vcpkg binary cache on GitHub Packages)
- `release.yml`: `contents: write` (semantic-release pushes commits/tags, creates releases) + `packages: write`
- `docker.yml`: `contents: read` + `packages: write` (push to GHCR)
- `docs.yml`: `contents: read` + `pages: write` + `id-token: write` (Pages deployment)

## Conventions

- **Never change CI without testing locally.** Equivalent commands are in the `Justfile` at the repo root (`just test`, `just coverage`, `just tidy`, etc.).
- **Preserve CMake presets.** Workflows use the presets defined in `CMakePresets.json` (`clang`, `clang-test`, `clang-lint`). Do not replace them with inline CMake commands.
- **The vcpkg binary cache is mandatory.** All build workflows use the `setup-vcpkg` action. Do not remove the cache — builds would be much slower.
- **Matrix `fail-fast: false`.** When adding a matrix job, always set `fail-fast: false` so all OSes run even if one fails.
- **Path filters.** `docker.yml` and `docs.yml` use path filters to avoid running unnecessarily. Add or modify them carefully.
- **Naming conventions.** Job names use the format `name (${{ matrix.os }})` or `name (${{ matrix.target }})` when a matrix is involved.
- **Dependabot**: do not change the schedule (weekly is a good compromise). Add a new ecosystem only if the project adopts a new dependency manager.
- **Semantic release**: never create version tags manually. Push conventional commits to `main` (`feat:` for minor, `fix:` for patch) and let `python-semantic-release` handle the rest. The version in `pyproject.toml`, `CMakeLists.txt`, `vcpkg.json` and `client/npm/package.json` is managed automatically — do not edit it by hand.
- **Conventional commits are required.** Commit messages must follow the conventional commits spec: `type(scope): description`. Types that trigger a release: `feat` (minor), `fix`/`perf` (patch). All other types (`chore`, `docs`, `ci`, `refactor`, etc.) do not trigger a release.
- **`[skip ci]` on release commits.** The semantic-release commit message includes `[skip ci]` to prevent re-triggering workflows. Do not remove it.

## Local equivalents

| CI action | Local command (from repo root) |
|---|---|
| CI build-test | `just test` |
| CI lint | `just tidy <files>` (targeted lint) — full CI lint is `cmake --preset clang-lint && cmake --build build/clang-lint --parallel 1` |
| Coverage | `just coverage` |
| Docker build | `docker build -f server/Dockerfile .` |
| Docs build | `mkdocs build --strict` |
| Release build | `just build-release` then package manually. For a real release: push a `feat:` or `fix:` commit to `main` — semantic-release handles versioning, changelog, tag, and GitHub release automatically. |
| Semantic release (dry-run) | `pip install python-semantic-release && semantic-release version --print` (shows the next version without applying) |
