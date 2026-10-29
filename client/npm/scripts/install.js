#!/usr/bin/env node
"use strict";

const { spawnSync } = require("child_process");
const fs = require("fs");
const https = require("https");
const path = require("path");

const pkg = require("../package.json");

const assets = {
  "linux-x64": "tunnelpp-client-linux-x64.tar.gz",
  "linux-arm64": "tunnelpp-client-linux-arm64.tar.gz",
  "darwin-arm64": "tunnelpp-client-darwin-arm64.tar.gz",
  "win32-x64": "tunnelpp-client-win32-x64.zip",
  "win32-arm64": "tunnelpp-client-win32-arm64.zip",
};

const key = `${process.platform}-${process.arch}`;
const asset = assets[key];
if (!asset) {
  console.error(`@theo-dep/tunnelpp: unsupported platform ${key}`);
  process.exit(1);
}

const repo = process.env.TUNNELPP_REPO || "theo-dep/tunnelpp";
const url = `https://github.com/${repo}/releases/download/v${pkg.version}/${asset}`;
const vendorDir = path.join(__dirname, "..", "vendor");
const archivePath = path.join(vendorDir, asset);
const binaryName =
  process.platform === "win32" ? "tunnelpp-client.exe" : "tunnelpp-client";
const binaryPath = path.join(vendorDir, binaryName);

function download(url, dest, redirects) {
  return new Promise((resolve, reject) => {
    https
      .get(url, (res) => {
        if (
          res.statusCode >= 300 &&
          res.statusCode < 400 &&
          res.headers.location
        ) {
          res.resume();
          if (redirects > 5) {
            reject(new Error("too many redirects"));
            return;
          }
          resolve(download(res.headers.location, dest, redirects + 1));
          return;
        }
        if (res.statusCode !== 200) {
          res.resume();
          reject(
            new Error(`download failed: HTTP ${res.statusCode} for ${url}`),
          );
          return;
        }
        const file = fs.createWriteStream(dest);
        res.pipe(file);
        file.on("finish", () => file.close(resolve));
        file.on("error", reject);
      })
      .on("error", reject);
  });
}

async function main() {
  if (fs.existsSync(binaryPath)) {
    return;
  }
  fs.mkdirSync(vendorDir, { recursive: true });
  console.log(`@theo-dep/tunnelpp: downloading ${url}`);
  await download(url, archivePath, 0);
  // tar (bsdtar on Windows) extracts both .tar.gz and .zip
  const tar = spawnSync("tar", ["-xf", archivePath, "-C", vendorDir], {
    stdio: "inherit",
  });
  if (tar.status !== 0) {
    throw new Error(`failed to extract ${asset} (is tar available?)`);
  }
  fs.rmSync(archivePath, { force: true });
  if (process.platform !== "win32") {
    fs.chmodSync(binaryPath, 0o755);
  }
  console.log(`@theo-dep/tunnelpp: installed ${binaryPath}`);
}

main().catch((error) => {
  console.error(`@theo-dep/tunnelpp: ${error.message}`);
  console.error(
    "Download the binary manually from https://github.com/theo-dep/tunnelpp/releases and place it in vendor/",
  );
  process.exit(1);
});
