"use strict";

// Tests for the public JS API (index.js startTunnel) and the CLI wrapper (bin/).
// Runs against a throwaway copy of index.js + bin/ in a temp dir: the vendor/ binary path is
// resolved relative to index.js, so no file is ever created in the repo. child_process.spawn
// is stubbed before index.js is loaded; the bin wrapper is exercised as a real subprocess.

const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("fs");
const path = require("path");
const cp = require("child_process");

const { tmpdir } = require("./helpers");

const pkgDir = path.join(__dirname, "..");
const work = path.join(tmpdir(), "pkg");
fs.mkdirSync(work, { recursive: true });
fs.copyFileSync(path.join(pkgDir, "index.js"), path.join(work, "index.js"));
fs.cpSync(path.join(pkgDir, "bin"), path.join(work, "bin"), { recursive: true });

const vendorDir = path.join(work, "vendor");
const binName = process.platform === "win32" ? "tunnelpp-client.exe" : "tunnelpp-client";
const vendorBinary = path.join(vendorDir, binName);
const binJs = path.join(work, "bin", "tunnelpp-client.js");

test.after(() => {
  cp.spawn = realSpawn;
  fs.rmSync(path.dirname(work), { recursive: true, force: true });
});

function installFakeBinary() {
  fs.mkdirSync(vendorDir, { recursive: true });
  fs.writeFileSync(vendorBinary, '#!/bin/sh\necho "ran:$@"\nexit 42\n', { mode: 0o755 });
}

const realSpawn = cp.spawn;
const spawnCalls = [];
cp.spawn = (...args) => {
  spawnCalls.push(args);
  return { pid: 1234 };
};
const { startTunnel } = require(work);

test("startTunnel requires options.server and options.token", () => {
  assert.throws(() => startTunnel(), /requires options\.server and options\.token/);
  assert.throws(() => startTunnel({ server: "ws://x" }), /requires options\.server and options\.token/);
  assert.throws(() => startTunnel({ token: "t" }), /requires options\.server and options\.token/);
});

test("startTunnel fails with a clear error when the binary is missing", () => {
  assert.throws(
    () => startTunnel({ server: "ws://x", token: "t" }),
    /binary not found/,
  );
});

test("bin wrapper exits 1 with a clear message when the binary is missing", () => {
  const r = cp.spawnSync(process.execPath, [binJs], { encoding: "utf8" });
  assert.equal(r.status, 1);
  assert.match(r.stderr, /binary not found/);
});

test("startTunnel spawns the binary with the expected CLI args and defaults", () => {
  installFakeBinary();
  const child = startTunnel({ server: "wss://tunnel.example.com", token: "secret" });
  assert.equal(child.pid, 1234, "startTunnel must return the spawned child");
  const [bin, args, opts] = spawnCalls.at(-1);
  assert.equal(bin, vendorBinary);
  assert.deepEqual(args, [
    "--server", "wss://tunnel.example.com",
    "--token", "secret",
    "--local", "http://127.0.0.1:8080",
  ]);
  assert.equal(opts.stdio, "inherit");
});

test("startTunnel forwards local, subdomain, maxReconnectDelay and verbose", () => {
  startTunnel({
    server: "ws://x",
    token: "t",
    local: "http://127.0.0.1:3000",
    subdomain: "my-app",
    maxReconnectDelay: 60,
    verbose: true,
  });
  const [, args] = spawnCalls.at(-1);
  assert.deepEqual(args, [
    "--server", "ws://x",
    "--token", "t",
    "--local", "http://127.0.0.1:3000",
    "--subdomain", "my-app",
    "--max-reconnect-delay", "60",
    "--verbose",
  ]);
});

test(
  "bin wrapper forwards argv to the binary and propagates its exit code",
  { skip: process.platform === "win32" ? "fake binary is a shell script" : false },
  () => {
    const r = cp.spawnSync(process.execPath, [binJs, "--server", "ws://x", "--token", "t"], {
      encoding: "utf8",
    });
    assert.equal(r.status, 42);
    assert.match(r.stdout, /ran:--server ws:\/\/x --token t/);
  },
);
