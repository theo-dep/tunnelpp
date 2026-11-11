"use strict";

// Sanity checks on package.json: the published shape must stay consistent
// (bin/main/files entries exist, no version in the repo, install script wired).

const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("fs");
const path = require("path");

const pkgDir = path.join(__dirname, "..");
const pkg = require("../package.json");

test("package metadata is consistent", () => {
  assert.equal(pkg.name, "@theo-dep/tunnelpp");
  assert.ok(!("version" in pkg), "no version in the repo: release.yml sets it at publish time");
  assert.equal(pkg.scripts.install, "node scripts/install.js");
  assert.equal(pkg.bin["tunnelpp-client"], "bin/tunnelpp-client.js");
  assert.ok(fs.existsSync(path.join(pkgDir, pkg.bin["tunnelpp-client"])), "bin entry must exist");
  assert.ok(fs.existsSync(path.join(pkgDir, pkg.main)), "main entry must exist");
  for (const entry of pkg.files) {
    if (entry === "vendor") continue; // created by the install script, not in the repo
    assert.ok(fs.existsSync(path.join(pkgDir, entry)), `files entry missing on disk: ${entry}`);
  }
  assert.ok(pkg.engines && pkg.engines.node, "engines.node must be declared");
});
