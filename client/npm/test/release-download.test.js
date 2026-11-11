"use strict";

// Integration test (network required, runs in the CI `npm-wrapper` job): download the real
// release asset for the current platform from GitHub and extract it with the install script.
// Complements the offline unit tests: proves the asset URL built from version.txt + the asset
// map actually exists, and that the real archives (GNU tar, Compress-Archive zip) extract.

const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("fs");
const path = require("path");

const { assets, download, extract } = require("../scripts/install.js");
const { tmpdir } = require("./helpers");

const repoRoot = path.join(__dirname, "..", "..", "..");

test(
  "real release asset downloads and extracts for the current platform",
  { timeout: 180_000 },
  async () => {
    const version = fs.readFileSync(path.join(repoRoot, "version.txt"), "utf8").trim();
    const key = `${process.platform}-${process.arch}`;
    const asset = assets[key];
    assert.ok(asset, `no asset mapped for ${key}`);
    const repo = process.env.TUNNELPP_REPO || "theo-dep/tunnelpp";
    const url = `https://github.com/${repo}/releases/download/v${version}/${asset}`;

    const dir = tmpdir();
    try {
      const archivePath = path.join(dir, asset);
      const vendorDir = path.join(dir, "vendor");
      fs.mkdirSync(vendorDir);
      await download(url, archivePath, 0);
      assert.ok(fs.statSync(archivePath).size > 0, "downloaded archive is empty");

      extract(archivePath, vendorDir);
      const binaryName =
        process.platform === "win32" ? "tunnelpp-client.exe" : "tunnelpp-client";
      const binary = fs.readFileSync(path.join(vendorDir, binaryName));
      // the win32 .exe is ~0.7 MB, the linux one ~9 MB: just rule out an HTML error page
      assert.ok(binary.length > 100_000, `suspiciously small binary: ${binary.length} bytes`);
      const magics =
        process.platform === "win32"
          ? [[0x4d, 0x5a]] // PE "MZ"
          : process.platform === "darwin"
            ? [
                [0xcf, 0xfa, 0xed, 0xfe], // Mach-O 64-bit
                [0xca, 0xfe, 0xba, 0xbe], // Mach-O universal (fat)
              ]
            : [[0x7f, 0x45, 0x4c, 0x46]]; // ELF
      const head = [...binary.subarray(0, 4)];
      assert.ok(
        magics.some((m) => m.every((b, i) => head[i] === b)),
        `unexpected binary format: ${head.map((b) => b.toString(16)).join(" ")}`,
      );
    } finally {
      fs.rmSync(dir, { recursive: true, force: true });
    }
  },
);
