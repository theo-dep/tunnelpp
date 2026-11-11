"use strict";

// Release/platform coverage: the asset map of scripts/install.js must stay in sync
// with the build matrix of .github/workflows/release.yml (see client/AGENTS.md "Do not").

const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("fs");
const path = require("path");

const { assets } = require("../scripts/install.js");
const pkg = require("../package.json");

const repoRoot = path.join(__dirname, "..", "..", "..");

test("asset map covers exactly the client targets of the release build matrix", () => {
  const releaseYml = fs.readFileSync(
    path.join(repoRoot, ".github", "workflows", "release.yml"),
    "utf8",
  );
  const targets = [
    ...releaseYml.matchAll(/-\s*\{[^}]*target:\s*([a-z0-9-]+)\s*\}/g),
  ].map((m) => m[1]);
  assert.ok(targets.length > 0, "no build matrix targets parsed from release.yml");
  // matrix targets look like linux-x64, linux-x64-ubuntu24, win32-x64-win2022, ...:
  // the npm package consumes one asset per canonical <platform>-<arch>
  const keys = [...new Set(targets.map((t) => t.split("-").slice(0, 2).join("-")))].sort();
  assert.deepEqual(Object.keys(assets).sort(), keys);
  for (const [key, asset] of Object.entries(assets)) {
    const ext = key.startsWith("win32-") ? "zip" : "tar.gz";
    assert.equal(asset, `tunnelpp-client-${key}.${ext}`, `wrong asset for ${key}`);
  }
});

test("release workflow still packages assets with the expected naming scheme", () => {
  const releaseYml = fs.readFileSync(
    path.join(repoRoot, ".github", "workflows", "release.yml"),
    "utf8",
  );
  // dist/tunnelpp-client-$env:TARGET.zip on Windows, dist/tunnelpp-client-$TARGET.tar.gz elsewhere
  assert.match(releaseYml, /tunnelpp-client-\$env:TARGET\.zip/);
  assert.match(releaseYml, /tunnelpp-client-\$TARGET\.tar\.gz/);
});

test("package.json os/cpu lists match the asset map (npm blocks install per os/cpu)", () => {
  const combos = pkg.os.flatMap((os) => pkg.cpu.map((cpu) => `${os}-${cpu}`)).sort();
  assert.deepEqual(Object.keys(assets).sort(), combos);
});
