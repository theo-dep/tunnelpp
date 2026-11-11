"use strict";

// End-to-end test (network required, runs in the CI `npm-wrapper` job): pack the package from
// this checkout, `npm install` the tarball into a scratch consumer project — which runs the
// real `install` lifecycle script and downloads the real release asset — then run the installed
// CLI and use the installed JS API.

const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("fs");
const path = require("path");
const { spawnSync } = require("child_process");

const { tmpdir } = require("./helpers");

const repoRoot = path.join(__dirname, "..", "..", "..");
const pkgDir = path.join(__dirname, "..");
const npmBin = process.platform === "win32" ? "npm.cmd" : "npm";

function run(cmd, args, options) {
  const r = spawnSync(cmd, args, {
    encoding: "utf8",
    // on Windows npm is npm.cmd: it must be spawned through a shell
    shell: process.platform === "win32",
    ...options,
  });
  assert.equal(
    r.status,
    0,
    `${cmd} ${args.join(" ")} failed (${r.error ?? r.status}):\n${r.stdout}\n${r.stderr}`,
  );
  return r;
}

test(
  "npm install of the packed package installs the release binary and works",
  { timeout: 300_000 },
  () => {
    const version = fs.readFileSync(path.join(repoRoot, "version.txt"), "utf8").trim();
    const dir = tmpdir();
    try {
      // pack a copy of client/npm with the release version set (as publish-npm does)
      const pkgCopy = path.join(dir, "pkg");
      fs.cpSync(pkgDir, pkgCopy, {
        recursive: true,
        // never pack a locally downloaded vendor/ (the install script fetches it)
        filter: (src) => path.relative(pkgDir, src) !== "vendor",
      });
      const pkgJsonPath = path.join(pkgCopy, "package.json");
      const pkgJson = JSON.parse(fs.readFileSync(pkgJsonPath, "utf8"));
      pkgJson.version = version;
      fs.writeFileSync(pkgJsonPath, JSON.stringify(pkgJson, null, 2) + "\n");
      const pack = run(npmBin, ["pack", "--pack-destination", dir], { cwd: pkgCopy });
      const tarball = path.join(dir, pack.stdout.trim().split("\n").pop());
      assert.ok(fs.existsSync(tarball), `tarball not found: ${tarball}`);

      // install the tarball into a scratch consumer project (runs the install script)
      const consumer = path.join(dir, "consumer");
      fs.mkdirSync(consumer);
      fs.writeFileSync(
        path.join(consumer, "package.json"),
        JSON.stringify({ name: "consumer", private: true }),
      );
      run(npmBin, ["install", tarball, "--no-audit", "--no-fund", "--no-ignore-scripts"], {
        cwd: consumer,
      });

      const installed = path.join(consumer, "node_modules", "@theo-dep", "tunnelpp");
      const binaryName =
        process.platform === "win32" ? "tunnelpp-client.exe" : "tunnelpp-client";
      const binaryPath = path.join(installed, "vendor", binaryName);
      assert.ok(fs.existsSync(binaryPath), "vendor binary missing after npm install");
      assert.ok(
        fs.existsSync(path.join(consumer, "node_modules", ".bin", "tunnelpp-client")),
        "bin shim missing in node_modules/.bin",
      );
      if (process.platform !== "win32") {
        assert.notEqual(fs.statSync(binaryPath).mode & 0o111, 0, "binary must be executable");
      }

      // the installed CLI runs the downloaded binary
      const help = run(process.execPath, [path.join(installed, "bin", "tunnelpp-client.js"), "--help"]);
      assert.match(help.stdout, /--server/);

      // the installed JS API is usable
      const api = require(installed);
      assert.equal(typeof api.startTunnel, "function");
      assert.throws(() => api.startTunnel({}), /requires options\.server and options\.token/);
    } finally {
      fs.rmSync(dir, { recursive: true, force: true });
    }
  },
);
