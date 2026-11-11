# AGENTS.md — CI/CD (GitHub Actions)

This file documents the CI/CD pipeline. It applies to everything under `.github/`:
workflows, composite actions, Dependabot configuration.

## Overview

| Workflow | File | Trigger | Purpose |
|---|---|---|---|
| CI | `workflows/ci.yml` | push to `main`, PR | Multi-OS build + tests, clang-tidy lint |
| Coverage | `workflows/coverage.yml` | push to `main`, manual | LLVM coverage report (artifact) |
| Docs | `workflows/docs.yml` | push to `main` (path-filtered), manual | Build MkDocs and deploy GitHub Pages |
| Release | `workflows/release.yml` | push to `main` | Semantic-release (conventional commits): binaries, npm package, Docker image |
| CodeQL | `workflows/codeql.yml` | push/PR to `main`, weekly | Code scanning (C++, JavaScript/TypeScript) |

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

### Docs (`docs.yml`)

- Triggered on push to `main` when `docs/**` or `mkdocs.yml` change, or manually
- Builds MkDocs with `mkdocs-material`, `--strict` mode (fails on warnings)
- Deploys to GitHub Pages via `actions/deploy-pages`
- Concurrency `pages` with `cancel-in-progress: true`

### Release (`release.yml`)

Two phases:

- **semantic-release** — runs on every push to `main`
  - Mints a GitHub App installation token (`actions/create-github-app-token@v3`, repo variable `APP_CLIENT_ID` + secret `APP_PRIVATE_KEY`): the release commit/tag push and the GitHub release are authenticated as the app bot (e.g. `semantic-release-bot[bot]`), a bypass actor of the `main` ruleset — no personal PAT
  - Installs semantic-release from the root `package.json` (`npm ci`) and analyzes commits since the last tag (`release.config.js`)
  - If `feat`, `fix` or `perf` commits are found: bumps the version in `version.txt` (`@semantic-release/exec`, the single source of truth), updates `CHANGELOG.md`, commits with `[skip ci]`, creates tag `v{version}`, pushes, and creates a GitHub release with the changelog
  - Outputs `released` (true/false) and `version` for downstream jobs
  - Version rules: `feat` → minor bump (0.x.y → 0.x+1.0), `fix`/`perf` → patch bump (0.x.y → 0.x.y+1). `chore`, `docs`, `ci`, etc. do not trigger a release.

- **publish-npm** — only runs when `released == 'true'`
  - Checks out the release tag `v{version}`
  - `client/npm/package.json` carries no version in the repo: the job sets it with `npm version --no-git-tag-version` before publishing (`client/npm/scripts/install.js` reads it to build the release asset URL)
  - Publishes `@theo-dep/tunnelpp` to GitHub Packages npm registry

- **publish-docker** — only runs when `released == 'true'`, after the `build` matrix
  - Checks out the release tag `v{version}`
  - QEMU + Buildx, multi-arch (`linux/amd64`, `linux/arm64`)
  - The `server/Dockerfile` packages the release binary (downloaded from the GitHub release, matched to
    `TARGETARCH`) instead of compiling. The packaged assets are the Ubuntu 26.04 builds (`linux-x64` /
    `linux-arm64`) on an `ubuntu:26.04` base image, so the glibc of the binary and of the base match.
    The base image is x86-64 baseline (the amd64v3 variant is opt-in, not enabled); the packaged
    binary is verified baseline by the check below
  - Tags `ghcr.io/theo-dep/tunnelpp`: `{version}` always; `{major}.{minor}` and `latest` only on stable releases (not prereleases)
  - `provenance: false` + `sbom: false`: attestation manifests are pushed by digest without platform, GHCR lists them as untagged `unknown/unknown` entries
  - GHA cache (`cache-from`/`cache-to` type=gha, mode=max)

- **build** — 11-target matrix, only runs when `released == 'true'`
  - Checks out the release tag `v{version}`
  - Release build with the `clang` preset (no sanitizers)
  - Linux x64 builds force the x86-64 baseline (the Ubuntu 26.04 toolchain targets x86-64-v3 by
    default, which makes binaries refuse to start on CPUs without AVX2), in two places:
    - project code: CMake option `TUNNELPP_X86_64_BASELINE` (OFF by default, ON in the `clang`
      preset in CMakePresets.json) adds `-march=x86-64 -mtune=generic`; build without the preset
      or with the option OFF to target v3
    - vcpkg dependencies: overlay triplet `triplets/x64-linux.cmake` (same name as the builtin
      `x64-linux`, resolved first via `VCPKG_OVERLAY_TRIPLETS`, set once in the `clang` preset and
      forwarded to the vcpkg CLI by the toolchain) adds the same flags to `VCPKG_C_FLAGS`/
      `VCPKG_CXX_FLAGS`. The flags live in the triplet (not in env vars) so they are part of the
      vcpkg ABI hash: stale v3 binaries are never reused from the NuGet cache, and the first build
      after adding the triplet recompiles all ports from source once
  - On Linux, the job pins the baseline `amd64` apt variant before installing the toolchain: the
    ubuntu-26.04 runner enables the `amd64v3` variant, whose `libc6-dev` marks every linked binary
    x86-64-v3 via its `crt1.o` note (the linker merges GNU property notes)
  - A `Check x86-64 baseline (linux x64)` step fails the linux x64 builds if a compile command drops
    `-march=x86-64`, a project object contains ymm/zmm code, or a linked binary's GNU property note
    (`readelf -nW`) requires more than the baseline — glibc refuses to start such binaries on CPUs
    without AVX2 even when the code itself is baseline
  - Packages as `.zip` (Windows, exe + the vcpkg runtime DLLs from `build/clang/vcpkg_installed/*/bin/`,
    required at runtime — the win32 build links vcpkg dynamically) or `.tar.gz` (others)
  - Uploads binaries to the GitHub release created by semantic-release
  - The supported platform list lives in 5 places that must stay in sync: this matrix (one base target
    per platform-arch), `client/npm/scripts/install.js` (asset map), `docs/client.md`,
    `client/npm/README.md` ("Supported platforms") and `server/Dockerfile` (linux x64/arm64 asset names).
    Any change to one updates the four others.

### CodeQL (`codeql.yml`)

- Triggered on push/PR to `main` and weekly (cron), per GitHub's template
- Matrix over two languages, `fail-fast: false`:
  - `c-cpp` with `build-mode: manual`: autobuild cannot work (the CMake configure needs vcpkg), so the job installs Ninja + Clang, sets up vcpkg (mandatory binary cache) and builds with the `clang` preset (`cmake --preset clang`, `cmake --build --preset clang-debug`) after `codeql-action/init` so the compilation is intercepted
  - `javascript-typescript` with `build-mode: none`: no build step (`client/npm/` wrapper, `release.config.js`)

## Semantic release configuration

`release.config.js` (semantic-release, Node.js; plugins installed from the root `package.json` via `npm ci`):

- **Version source of truth**: the git tag. The files below are kept in sync by the release commit
- **Branches**: `main` is a prerelease branch (`prerelease: "alpha"` → versions `x.y.z-alpha.N`). `stable` is also declared because semantic-release requires at least one release branch; it must exist on the remote but is never pushed to — all releases are prereleases from `main`. If `stable` ever disappears (e.g. branch cleanup), recreate it with `git push origin origin/main:refs/heads/stable` — otherwise the release job fails with `ERELEASEBRANCHES`
- **Tag format**: `v{version}` (e.g. `v0.2.0`)
- **Commit analyzer**: conventional commits; only `feat` (minor), `fix`/`perf` (patch) trigger a release
- **Changelog**: `CHANGELOG.md`; only `feat`/`fix`/`perf` commits are listed, all other types are hidden
- **Version source**: `version.txt` is the single source of truth, bumped by `@semantic-release/exec` (`echo "${nextRelease.version}" > version.txt`). `CMakeLists.txt` reads it at configure time and strips the prerelease suffix for `project(tunnelpp VERSION ...)`; `vcpkg.json` and `client/npm/package.json` carry no version in the repo (the `publish-npm` job sets the latter at publish time)
- **Release commit**: `@semantic-release/git` commits `version.txt` + `CHANGELOG.md` with `chore(release): ${nextRelease.version} [skip ci]` — the `[skip ci]` prevents re-triggering all workflows on the version-bump commit
- **GitHub release**: `@semantic-release/github`

Pitfalls (each one has broken CI once):

- Plugin templates receive `{ branch, lastRelease, nextRelease }` — use `${nextRelease.version}`, never `${version}` (the `@semantic-release/git` commit message crashes with `ReferenceError: version is not defined` otherwise)
- `conventional-changelog-conventionalcommits` must stay on v9 (`^9.3.1`): v10 requires `conventional-changelog-writer@9`, but `@semantic-release/changelog@7` / `release-notes-generator@14` ship writer@8 (handlebars `helper-missing` crash when rendering notes)
- `--dry-run` skips `prepare`/`publish`, so it does not exercise the plugins' write steps: before changing `release.config.js`, test each plugin step (`exec`, `git`, `changelog`, `npm`) individually in a scratch clone, and run the full `--dry-run` for the version calculation

### GitHub App (bot) authentication

The release job uses no personal PAT. Create a GitHub App (e.g. `semantic-release-bot`) with
**Contents: read/write**, **Pull requests: write**, **Issues: write** and **Metadata: read** (the
PR/Issues write permissions are needed because `@semantic-release/github` comments on the PRs and
issues included in each release — without them the job logs `Not allowed to add a comment to the
issue/PR` for each of them), install it on the repo, add its
`semantic-release-bot[bot]` account as a **bypass actor** of the `main` ruleset (and remove
yourself from the bypass list). The client id is not secret: store it as the repo **variable**
`APP_CLIENT_ID` (Settings → Variables → Repository variables); the private key goes in the repo
**secret** `APP_PRIVATE_KEY`. The workflow mints an installation token with
`actions/create-github-app-token@v3`, so the release commit, tag and GitHub release are authored
by the bot.

### One-time migration from python-semantic-release

semantic-release links tags to branches through git notes (`refs/notes/semantic-release-*`).
The existing tags were created by python-semantic-release and have no notes, so without them
semantic-release would not find the last release and the next version would restart from
`1.0.0-alpha.1` instead of `0.1.0-alpha.6`. Add the channel note to every existing tag once:

```bash
for t in $(git tag); do
  git notes --ref refs/notes/semantic-release-main add -f -m '{"channels":["main"]}' "$t^{commit}"
done
git push origin 'refs/notes/semantic-release-*'
```

## Dependabot

`.github/dependabot.yml` — weekly automatic updates with conventional commit messages:

| Ecosystem | Directory | Schedule | Commit prefix | Release? |
|---|---|---|---|---|
| `github-actions` | `/` | weekly | `chore(deps)` | no |
| `npm` | `/` (semantic-release tooling) | weekly | `chore(deps)` | no |
| `npm` | `/client/npm` | weekly | `chore(deps)` | no |
| `vcpkg` | `/` | weekly | `fix(deps)` | **yes** (patch bump) |
| `docker` | `/server` | weekly | `chore(deps)` | no |

vcpkg uses `fix(deps)` because C++ dependency updates often include security fixes (OpenSSL, httplib, etc.) that warrant a patch release. All other ecosystems use `chore(deps)` and never trigger a version bump. Example messages:
- `fix(deps): bump openssl from 3.3.0 to 3.3.1`
- `chore(deps): bump actions/checkout from 4 to 5`

## Composite action: `setup-vcpkg`

`.github/actions/setup-vcpkg/action.yml` — used by `ci.yml`, `coverage.yml` and `release.yml`.

- Checks out `microsoft/vcpkg` into `.vcpkg/`, pinned to the `builtin-baseline` commit of `vcpkg.json` (depth-1 fetch)
- Bootstraps vcpkg (`.sh` on Linux/macOS, `.bat` on Windows)
- Installs `mono` on Linux/macOS (needed to run `nuget.exe`)
- Configures the vcpkg binary cache on a NuGet feed hosted on GitHub Packages
- Uses `secrets.GITHUB_TOKEN` for authentication
- Environment variables expected by the action: `VCPKG_ROOT`, `VCPKG_EXE`, `VCPKG_NUGET_FEED_URL` (set at the `env:` level in each workflow). `VCPKG_BINARY_SOURCES` is set by the action itself via `GITHUB_ENV` (NuGet cache where mono is installable, `clear` fallback on runners where the focal Mono packages have unmet dependencies, e.g. Ubuntu 26.04 — dependencies then build from source)

## Permissions

All workflows follow the principle of least privilege:

- `ci.yml`, `coverage.yml`: `contents: read` + `packages: write` (for the vcpkg binary cache on GitHub Packages)
- `codeql.yml`: `contents: read` + `packages: write` (CodeQL query packs + vcpkg binary cache) + `security-events: write` (SARIF upload to code scanning)
- `release.yml`: `contents: write` (semantic-release pushes commits/tags, creates releases) + `packages: write` (vcpkg binary cache, npm registry, GHCR push)
- `docs.yml`: `contents: read` + `pages: write` + `id-token: write` (Pages deployment)

## Conventions

- **Never change CI without testing locally.** Equivalent commands are in the `Justfile` at the repo root (`just test`, `just coverage`, `just tidy`, etc.).
- **Preserve CMake presets.** Workflows use the presets defined in `CMakePresets.json` (`clang`, `clang-test`, `clang-lint`). Do not replace them with inline CMake commands.
- **The vcpkg binary cache is mandatory.** All build workflows use the `setup-vcpkg` action. Do not remove the cache — builds would be much slower.
- **Matrix `fail-fast: false`.** When adding a matrix job, always set `fail-fast: false` so all OSes run even if one fails.
- **Path filters.** `docs.yml` uses path filters to avoid running unnecessarily. Add or modify them carefully. The Docker image is published only on releases (`release.yml`), never on plain pushes to `main`.
- **Naming conventions.** Job names use the format `name (${{ matrix.os }})` or `name (${{ matrix.target }})` when a matrix is involved.
- **Dependabot**: do not change the schedule (weekly is a good compromise). Add a new ecosystem only if the project adopts a new dependency manager.
- **Semantic release**: never create version tags manually. Push conventional commits to `main` (`feat:` for minor, `fix:` for patch) and let `semantic-release` handle the rest. The version in `version.txt` is managed automatically (CMake reads it at configure time) — do not edit it by hand. `client/npm/package.json` has no `version` field on purpose: it is set at publish time.
- **Conventional commits are required.** Commit messages must follow the conventional commits spec: `type(scope): description`. Types that trigger a release: `feat` (minor), `fix`/`perf` (patch). All other types (`chore`, `docs`, `ci`, `refactor`, etc.) do not trigger a release.
- **`[skip ci]` on release commits.** The semantic-release commit message includes `[skip ci]` to prevent re-triggering workflows. Do not remove it.

## Local equivalents

| CI action | Local command (from repo root) |
|---|---|
| CI build-test | `just test` |
| CI lint | `just tidy <files>` (targeted lint) — full CI lint is `cmake --preset clang-lint && cmake --build build/clang-lint --parallel 1` |
| Coverage | `just coverage` |
| Docker build | `docker build --build-arg VERSION=<released version> -f server/Dockerfile .` (the release must already be published) |
| Docs build | `mkdocs build --strict` |
| Release build | `just build-release` then package manually. For a real release: push a `feat:` or `fix:` commit to `main` — semantic-release handles versioning, changelog, tag, and GitHub release automatically. |
| Semantic release (dry-run) | `npm ci && npx semantic-release --dry-run` (shows the next version without applying) |
