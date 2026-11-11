#!/usr/bin/env node
"use strict";

const fs = require("fs");
const https = require("https");
const path = require("path");
const zlib = require("zlib");

const pkg = require("../package.json");

const assets = {
  "linux-x64": "tunnelpp-client-linux-x64.tar.gz",
  "linux-arm64": "tunnelpp-client-linux-arm64.tar.gz",
  "darwin-arm64": "tunnelpp-client-darwin-arm64.tar.gz",
  "darwin-x64": "tunnelpp-client-darwin-x64.tar.gz",
  "win32-x64": "tunnelpp-client-win32-x64.zip",
  "win32-arm64": "tunnelpp-client-win32-arm64.zip",
};

// Extraction is pure Node, no external tar/unzip: on Windows the `tar` found in PATH is often
// GNU tar (Git for Windows), which cannot read .zip and misreads `D:\...\@scope\...` paths as a
// remote host ("Cannot connect to ...: resolve failed").

function safeJoin(destDir, name) {
  const dest = path.resolve(destDir, name);
  if (dest !== destDir && !dest.startsWith(destDir + path.sep)) {
    throw new Error(`unsafe path in archive: ${name}`);
  }
  return dest;
}

function extractTarGz(archivePath, destDir) {
  const buf = zlib.gunzipSync(fs.readFileSync(archivePath));
  let offset = 0;
  while (offset + 512 <= buf.length) {
    const header = buf.subarray(offset, offset + 512);
    if (header.every((byte) => byte === 0)) {
      break;
    }
    const field = (start, end) =>
      header.subarray(start, end).toString("utf8").split("\0")[0];
    let name = field(0, 100);
    const prefix = field(345, 500);
    if (prefix) {
      name = `${prefix}/${name}`;
    }
    const sizeField = field(124, 136).trim();
    const size = sizeField ? parseInt(sizeField, 8) : 0;
    const typeflag = String.fromCharCode(header[156]);
    offset += 512;
    if (typeflag === "0" || typeflag === "\0") {
      const dest = safeJoin(destDir, name);
      fs.mkdirSync(path.dirname(dest), { recursive: true });
      fs.writeFileSync(dest, buf.subarray(offset, offset + size));
    }
    offset += Math.ceil(size / 512) * 512;
  }
}

function extractZip(archivePath, destDir) {
  const buf = fs.readFileSync(archivePath);
  let eocd = -1;
  const min = Math.max(0, buf.length - 22 - 0xffff);
  for (let i = buf.length - 22; i >= min; i--) {
    if (buf.readUInt32LE(i) === 0x06054b50) {
      eocd = i;
      break;
    }
  }
  if (eocd < 0) {
    throw new Error("not a zip archive");
  }
  const count = buf.readUInt16LE(eocd + 10);
  let offset = buf.readUInt32LE(eocd + 16);
  for (let n = 0; n < count; n++) {
    if (buf.readUInt32LE(offset) !== 0x02014b50) {
      throw new Error("corrupt zip central directory");
    }
    const method = buf.readUInt16LE(offset + 10);
    const compressedSize = buf.readUInt32LE(offset + 20);
    const nameLen = buf.readUInt16LE(offset + 28);
    const extraLen = buf.readUInt16LE(offset + 30);
    const commentLen = buf.readUInt16LE(offset + 32);
    const localOffset = buf.readUInt32LE(offset + 42);
    const name = buf.subarray(offset + 46, offset + 46 + nameLen).toString("utf8");
    offset += 46 + nameLen + extraLen + commentLen;
    if (name.endsWith("/")) {
      continue;
    }
    if (buf.readUInt32LE(localOffset) !== 0x04034b50) {
      throw new Error(`corrupt zip local header for ${name}`);
    }
    const localNameLen = buf.readUInt16LE(localOffset + 26);
    const localExtraLen = buf.readUInt16LE(localOffset + 28);
    const start = localOffset + 30 + localNameLen + localExtraLen;
    const data = buf.subarray(start, start + compressedSize);
    let content;
    if (method === 0) {
      content = data;
    } else if (method === 8) {
      content = zlib.inflateRawSync(data);
    } else {
      throw new Error(`unsupported compression method ${method} for ${name}`);
    }
    const dest = safeJoin(destDir, name);
    fs.mkdirSync(path.dirname(dest), { recursive: true });
    fs.writeFileSync(dest, content);
  }
}

function extract(archivePath, destDir) {
  if (archivePath.endsWith(".zip")) {
    extractZip(archivePath, destDir);
  } else {
    extractTarGz(archivePath, destDir);
  }
}

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

async function main(options = {}) {
  const key = `${process.platform}-${process.arch}`;
  const asset = assets[key];
  if (!asset) {
    throw new Error(`unsupported platform ${key}`);
  }
  const repo = process.env.TUNNELPP_REPO || "theo-dep/tunnelpp";
  const url = `https://github.com/${repo}/releases/download/v${pkg.version}/${asset}`;
  const vendorDir = options.vendorDir || path.join(__dirname, "..", "vendor");
  const archivePath = path.join(vendorDir, asset);
  const binaryName =
    process.platform === "win32" ? "tunnelpp-client.exe" : "tunnelpp-client";
  const binaryPath = path.join(vendorDir, binaryName);
  if (fs.existsSync(binaryPath)) {
    return;
  }
  fs.mkdirSync(vendorDir, { recursive: true });
  console.log(`@theo-dep/tunnelpp: downloading ${url}`);
  await download(url, archivePath, 0);
  extract(archivePath, vendorDir);
  fs.rmSync(archivePath, { force: true });
  if (process.platform !== "win32") {
    fs.chmodSync(binaryPath, 0o755);
  }
  console.log(`@theo-dep/tunnelpp: installed ${binaryPath}`);
}

if (require.main === module) {
  main().catch((error) => {
    console.error(`@theo-dep/tunnelpp: ${error.message}`);
    console.error(
      "Download the binary manually from https://github.com/theo-dep/tunnelpp/releases and place it in vendor/",
    );
    process.exit(1);
  });
}

module.exports = { assets, download, extract, extractTarGz, extractZip, main };
