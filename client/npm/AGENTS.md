# AGENTS.md — npm package

`@theo-dep/tunnelpp`: npm wrapper around the `tunnelpp-client` binary. The root `AGENTS.md` and
`client/AGENTS.md` apply; this file wins on conflict for everything under `client/npm/`.

## Layout

- `package.json` — no `version` field in the repo: the `publish-npm` job of
  `.github/workflows/release.yml` sets it at publish time (`npm version --no-git-tag-version`).
- `index.js` — JS API: `startTunnel(options)` spawns `vendor/tunnelpp-client[.exe]`.
- `bin/tunnelpp-client.js` — CLI entry (the `bin` of package.json).
- `scripts/install.js` — `install` script: downloads the release asset
  `tunnelpp-client-<platform>-<arch>.tar.gz` (`.zip` on win32) from
  `https://github.com/<TUNNELPP_REPO or theo-dep/tunnelpp>/releases/download/v<version>/` and
  extracts it into `vendor/` **in pure Node** (zlib + minimal tar/zip parsing, path-traversal
  guarded). Never shell out to `tar`/`unzip`: on Windows the `tar` in PATH is often GNU tar (Git
  for Windows), which cannot read `.zip` and misreads `D:\...` paths as a remote host (issue #27).
- `test/` — `node:test` tests, no external dependency. Run `npm test` in this directory, or from
  the repo root `node --test "client/npm/test/*.test.js"` (CI job `npm-wrapper`, 3 OS):
  - `install.test.js`: tar.gz/zip extraction, path traversal rejection
  - `main.test.js`: download (redirects, HTTP errors) and full `main()` flow per platform,
    including a simulated win32 run
  - `release.test.js`: asset map ↔ `release.yml` build matrix, os/cpu ↔ assets
  - `release-download.test.js`: integration, needs network — downloads and extracts the real
    release asset for the current platform (`v{version.txt}`), checks the binary magic bytes
  - `npm-install.test.js`: end-to-end, needs network — `npm pack` a copy of the package (with
    `version.txt` as version, `vendor/` excluded), `npm install` the tarball into a scratch
    consumer project, then run the installed CLI (`--help`) and JS API. Runs the real `install`
    lifecycle script, i.e. the exact flow of issue #27. On win32 the `--help` run is conditional:
    the exe only executes if the downloaded asset ships the vcpkg runtime DLLs (assets published
    before the `release.yml` packaging fix crash at startup); the DLL packaging itself is pinned
    statically by `release.test.js`
  - `api.test.js`: `startTunnel` arg validation/spawn, `bin/` wrapper exit codes
  - `package.test.js`: package.json sanity (bin/main/files exist, no version in the repo)

## Rules

- Never add a dependency (`dependencies`/`devDependencies`) without asking.
- Edit `package.json` only through npm (`npm pkg set ...`), never by hand.
- `engines.node >= 16` applies to the published scripts (`install.js`, `index.js`, `bin/`);
  the tests need Node >= 18 (`node --test`).
- `files` must not list `test/` (tests are not published); `vendor/` is created at install time
  and gitignored.
- The supported platform list lives in 5 places, keep them in sync: the `build` matrix of
  `release.yml`, `assets` in `scripts/install.js`, `docs/client.md`, this `README.md` ("Supported
  platforms"), `server/Dockerfile` (linux server assets). `test/release.test.js` enforces
  `release.yml` ↔ `assets` automatically.
- Publish only through `release.yml` (`publish-npm` job) — never `npm publish` by hand, never
  create version tags manually.
