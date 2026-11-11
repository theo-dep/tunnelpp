"use strict";

// Extraction tests for scripts/install.js (the pure-Node tar.gz/zip extractors).

const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("fs");
const path = require("path");

const { extract, extractTarGz, extractZip } = require("../scripts/install.js");
const { buildTarGz, buildZip, tmpdir } = require("./helpers");

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
