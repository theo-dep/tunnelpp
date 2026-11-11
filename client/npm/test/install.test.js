#!/usr/bin/env node
"use strict";

// Tests for scripts/install.js (node:test, no external dependency). Run from the repo root:
//   node --test client/npm/test/
// or from client/npm: npm test

const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("fs");
const os = require("os");
const path = require("path");
const zlib = require("zlib");

const { assets, extract, extractTarGz, extractZip } = require("../scripts/install.js");

const repoRoot = path.join(__dirname, "..", "..", "..");

function tmpdir() {
  return fs.mkdtempSync(path.join(os.tmpdir(), "tunnelpp-install-test-"));
}

// --- archive builders (same formats as the release assets: GNU tar .tar.gz, .zip on Windows) ---

function tarEntry(name, content, typeflag) {
  const header = Buffer.alloc(512, 0);
  header.write(name, 0, 100, "utf8");
  header.write("0000755\0", 100, "ascii");
  header.write("0000000\0", 108, "ascii");
  header.write("0000000\0", 116, "ascii");
  header.write(content.length.toString(8).padStart(11, "0") + "\0", 124, "ascii");
  header.write("00000000000\0", 136, "ascii");
  header.write("        ", 148, "ascii");
  header.write(typeflag, 156, "ascii");
  header.write("ustar\0", 257, "ascii");
  header.write("00", 263, "ascii");
  let checksum = 0;
  for (const byte of header) checksum += byte;
  header.write(checksum.toString(8).padStart(6, "0") + "\0 ", 148, "ascii");
  const data = Buffer.alloc(Math.ceil(content.length / 512) * 512, 0);
  content.copy(data);
  return Buffer.concat([header, data]);
}

function buildTarGz(entries) {
  const parts = entries.map(([name, content, typeflag]) =>
    tarEntry(name, content, typeflag),
  );
  parts.push(Buffer.alloc(1024, 0));
  return zlib.gzipSync(Buffer.concat(parts));
}

function buildZip(entries) {
  const locals = [];
  const centrals = [];
  let offset = 0;
  for (const [name, content, method] of entries) {
    const nameBuf = Buffer.from(name, "utf8");
    const data = method === 8 ? zlib.deflateRawSync(content) : content;
    const localHeader = Buffer.alloc(30);
    localHeader.writeUInt32LE(0x04034b50, 0);
    localHeader.writeUInt16LE(20, 4);
    localHeader.writeUInt16LE(method, 8);
    localHeader.writeUInt32LE(0, 14);
    localHeader.writeUInt32LE(data.length, 18);
    localHeader.writeUInt32LE(content.length, 22);
    localHeader.writeUInt16LE(nameBuf.length, 26);
    const local = Buffer.concat([localHeader, nameBuf, data]);
    const central = Buffer.alloc(46);
    central.writeUInt32LE(0x02014b50, 0);
    central.writeUInt16LE(20, 4);
    central.writeUInt16LE(20, 6);
    central.writeUInt16LE(method, 10);
    central.writeUInt32LE(0, 14);
    central.writeUInt32LE(data.length, 20);
    central.writeUInt32LE(content.length, 24);
    central.writeUInt16LE(nameBuf.length, 28);
    central.writeUInt32LE(offset, 42);
    locals.push(local);
    centrals.push(Buffer.concat([central, nameBuf]));
    offset += local.length;
  }
  const centralDir = Buffer.concat(centrals);
  const eocd = Buffer.alloc(22);
  eocd.writeUInt32LE(0x06054b50, 0);
  eocd.writeUInt16LE(entries.length, 8);
  eocd.writeUInt16LE(entries.length, 10);
  eocd.writeUInt32LE(centralDir.length, 12);
  eocd.writeUInt32LE(offset, 16);
  return Buffer.concat([...locals, centralDir, eocd]);
}

// --- extraction ---

test("extracts a .tar.gz release asset (dirs, files, binary content)", () => {
  const dir = tmpdir();
  const binary = Buffer.from([0x7f, 0x45, 0x4c, 0x46, 0x00, 0xff, 0x01, 0x02]);
  const archive = path.join(dir, "tunnelpp-client-linux-x64.tar.gz");
  fs.writeFileSync(
    archive,
    buildTarGz([
      ["sub/", Buffer.alloc(0), "5"],
      ["tunnelpp-client", binary, "0"],
      ["sub/note.txt", Buffer.from("hello"), "0"],
    ]),
  );
  const dest = path.join(dir, "out");
  fs.mkdirSync(dest);
  extract(archive, dest);
  assert.deepEqual(fs.readFileSync(path.join(dest, "tunnelpp-client")), binary);
  assert.equal(fs.readFileSync(path.join(dest, "sub", "note.txt"), "utf8"), "hello");
});

test("extracts a .zip release asset (deflate and stored entries)", () => {
  const dir = tmpdir();
  const exe = Buffer.from([0x4d, 0x5a, 0x00, 0xff, 0x10, 0x20, 0x30, 0x40]);
  const archive = path.join(dir, "tunnelpp-client-win32-x64.zip");
  fs.writeFileSync(
    archive,
    buildZip([
      ["tunnelpp-client.exe", exe, 8],
      ["stored.txt", Buffer.from("stored"), 0],
    ]),
  );
  const dest = path.join(dir, "out");
  fs.mkdirSync(dest);
  extract(archive, dest);
  assert.deepEqual(fs.readFileSync(path.join(dest, "tunnelpp-client.exe")), exe);
  assert.equal(fs.readFileSync(path.join(dest, "stored.txt"), "utf8"), "stored");
});

test("rejects path traversal entries in tar and zip archives", () => {
  const dir = tmpdir();
  const tar = path.join(dir, "evil.tar.gz");
  fs.writeFileSync(tar, buildTarGz([["../evil.txt", Buffer.from("x"), "0"]]));
  assert.throws(() => extractTarGz(tar, path.join(dir, "out")), /unsafe path/);
  const zip = path.join(dir, "evil.zip");
  fs.writeFileSync(zip, buildZip([["../evil.txt", Buffer.from("x"), 8]]));
  assert.throws(() => extractZip(zip, path.join(dir, "out")), /unsafe path/);
});

// --- release platform coverage ---

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

test("every asset name follows the release naming scheme", () => {
  const releaseYml = fs.readFileSync(
    path.join(repoRoot, ".github", "workflows", "release.yml"),
    "utf8",
  );
  // the workflow packages dist/tunnelpp-client-$env:TARGET.zip on Windows, $TARGET.tar.gz elsewhere
  assert.match(releaseYml, /tunnelpp-client-\$env:TARGET\.zip/);
  assert.match(releaseYml, /tunnelpp-client-\$TARGET\.tar\.gz/);
});
