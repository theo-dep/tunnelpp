"use strict";

// Archive builders for the tests: same formats as the release assets
// (GNU tar .tar.gz on linux/darwin, .zip on win32).

const fs = require("fs");
const https = require("https");
const os = require("os");
const path = require("path");
const zlib = require("zlib");

// Release asset of a given version exists? github.com answers 302 (redirect to the CDN)
// for existing assets and 404 for missing ones: no need to follow the redirect.
function releaseAssetExists(url) {
  return new Promise((resolve, reject) => {
    https
      .get(url, { headers: { "User-Agent": "tunnelpp-npm-test" } }, (res) => {
        res.resume();
        resolve(res.statusCode >= 200 && res.statusCode < 400);
      })
      .on("error", reject);
  });
}

function fetchJson(url) {
  return new Promise((resolve, reject) => {
    https
      .get(url, { headers: { "User-Agent": "tunnelpp-npm-test" } }, (res) => {
        if (res.statusCode !== 200) {
          res.resume();
          reject(new Error(`HTTP ${res.statusCode} for ${url}`));
          return;
        }
        const chunks = [];
        res.on("data", (c) => chunks.push(c));
        res.on("end", () => resolve(JSON.parse(Buffer.concat(chunks).toString("utf8"))));
      })
      .on("error", reject);
  });
}

// Prefer v{preferredVersion} (version.txt); if that release misses the asset, fall back to
// the newest release that ships it.
async function resolveReleaseVersion(repo, asset, preferredVersion) {
  const exists = await releaseAssetExists(
    `https://github.com/${repo}/releases/download/v${preferredVersion}/${asset}`,
  );
  if (exists) {
    return preferredVersion;
  }
  const releases = await fetchJson(
    `https://api.github.com/repos/${repo}/releases?per_page=100`,
  );
  for (const release of releases) {
    if (release.assets.some((a) => a.name === asset)) {
      const version = release.tag_name.replace(/^v/, "");
      console.log(
        `asset ${asset} missing from v${preferredVersion}: falling back to v${version}`,
      );
      return version;
    }
  }
  throw new Error(`no release of ${repo} ships ${asset} (v${preferredVersion} included)`);
}

function tmpdir() {
  // realpath: on macOS os.tmpdir() is /var/... (symlink) while Node resolves module
  // __dirname to /private/var/... — paths must be compared on the same form
  return fs.realpathSync(fs.mkdtempSync(path.join(os.tmpdir(), "tunnelpp-npm-test-")));
}

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

module.exports = { buildTarGz, buildZip, tmpdir, resolveReleaseVersion };
