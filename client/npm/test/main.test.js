"use strict";

// Tests for the download and the full install flow of scripts/install.js.
// https.get is stubbed: no network access in tests.

const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("fs");
const path = require("path");
const https = require("https");
const { PassThrough } = require("stream");

const { assets, download, main } = require("../scripts/install.js");
const { buildTarGz, buildZip, tmpdir } = require("./helpers");

function fakeResponse({ statusCode = 200, headers = {}, body = Buffer.alloc(0) }) {
  const res = new PassThrough();
  res.statusCode = statusCode;
  res.headers = headers;
  setImmediate(() => res.end(body));
  return res;
}

function stubHttps(handler) {
  const original = https.get;
  const calls = [];
  https.get = (url, cb) => {
    calls.push(url);
    process.nextTick(() => cb(handler(url)));
    return { on: () => {} };
  };
  return { calls, restore: () => (https.get = original) };
}

async function withPlatform(platform, arch, fn) {
  const originalPlatform = Object.getOwnPropertyDescriptor(process, "platform");
  const originalArch = Object.getOwnPropertyDescriptor(process, "arch");
  Object.defineProperty(process, "platform", { value: platform });
  if (arch) {
    Object.defineProperty(process, "arch", { value: arch });
  }
  try {
    return await fn();
  } finally {
    Object.defineProperty(process, "platform", originalPlatform);
    if (arch) {
      Object.defineProperty(process, "arch", originalArch);
    }
  }
}

function buildAsset(platform, content) {
  const name = platform === "win32" ? "tunnelpp-client.exe" : "tunnelpp-client";
  return platform === "win32"
    ? buildZip([[name, content, 8]])
    : buildTarGz([[name, content, "0"]]);
}

test("download writes the response body to the destination", async () => {
  const dest = path.join(tmpdir(), "a.bin");
  const stub = stubHttps(() => fakeResponse({ body: Buffer.from("hello") }));
  try {
    await download("https://example.com/a", dest, 0);
  } finally {
    stub.restore();
  }
  assert.equal(fs.readFileSync(dest, "utf8"), "hello");
});

test("download follows redirects (github.com 302 -> CDN)", async () => {
  const dest = path.join(tmpdir(), "a.bin");
  const stub = stubHttps((url) =>
    url === "https://example.com/a"
      ? fakeResponse({ statusCode: 302, headers: { location: "https://cdn.example.com/a" } })
      : fakeResponse({ body: Buffer.from("cdn-body") }),
  );
  try {
    await download("https://example.com/a", dest, 0);
  } finally {
    stub.restore();
  }
  assert.deepEqual(stub.calls, ["https://example.com/a", "https://cdn.example.com/a"]);
  assert.equal(fs.readFileSync(dest, "utf8"), "cdn-body");
});

test("download rejects on HTTP error", async () => {
  const stub = stubHttps(() => fakeResponse({ statusCode: 404 }));
  try {
    await assert.rejects(
      download("https://example.com/x", path.join(tmpdir(), "x"), 0),
      /HTTP 404/,
    );
  } finally {
    stub.restore();
  }
});

test("download rejects after too many redirects", async () => {
  const stub = stubHttps(() =>
    fakeResponse({ statusCode: 302, headers: { location: "https://example.com/loop" } }),
  );
  try {
    await assert.rejects(
      download("https://example.com/loop", path.join(tmpdir(), "x"), 0),
      /too many redirects/,
    );
  } finally {
    stub.restore();
  }
});

test("main downloads, extracts and installs the asset for the current platform", async () => {
  const platform = process.platform;
  const key = `${platform}-${process.arch}`;
  const asset = assets[key];
  assert.ok(asset, `no asset mapped for ${key}`);
  const vendorDir = path.join(tmpdir(), "vendor");
  const content = Buffer.from(`fake ${key} binary`);
  const stub = stubHttps(() => fakeResponse({ body: buildAsset(platform, content) }));
  try {
    await main({ vendorDir });
    await main({ vendorDir }); // idempotent: no second download
  } finally {
    stub.restore();
  }
  assert.equal(stub.calls.length, 1);
  assert.match(stub.calls[0], /\/releases\/download\/v[^/]+\//);
  assert.ok(stub.calls[0].endsWith(`/${asset}`), `url should end with ${asset}: ${stub.calls[0]}`);
  const binaryName = platform === "win32" ? "tunnelpp-client.exe" : "tunnelpp-client";
  const binaryPath = path.join(vendorDir, binaryName);
  assert.deepEqual(fs.readFileSync(binaryPath), content);
  assert.ok(!fs.existsSync(path.join(vendorDir, asset)), "archive must be removed after extraction");
  if (platform !== "win32") {
    assert.equal(fs.statSync(binaryPath).mode & 0o777, 0o755, "binary must be executable");
  }
});

test("main installs the win32 .zip asset as tunnelpp-client.exe (issue #27)", async () => {
  await withPlatform("win32", "x64", async () => {
    const vendorDir = path.join(tmpdir(), "vendor");
    const content = Buffer.from("fake win32 binary");
    const stub = stubHttps(() => fakeResponse({ body: buildAsset("win32", content) }));
    try {
      await main({ vendorDir });
    } finally {
      stub.restore();
    }
    assert.ok(stub.calls[0].endsWith("/tunnelpp-client-win32-x64.zip"), stub.calls[0]);
    assert.deepEqual(
      fs.readFileSync(path.join(vendorDir, "tunnelpp-client.exe")),
      content,
    );
    assert.ok(!fs.existsSync(path.join(vendorDir, "tunnelpp-client-win32-x64.zip")));
  });
});

test("main rejects on an unsupported platform", async () => {
  await withPlatform("freebsd", undefined, async () => {
    await assert.rejects(
      main({ vendorDir: path.join(tmpdir(), "vendor") }),
      /unsupported platform freebsd/,
    );
  });
});

test("main rejects when the release asset does not exist (HTTP 404)", async () => {
  const stub = stubHttps(() => fakeResponse({ statusCode: 404 }));
  try {
    await assert.rejects(
      main({ vendorDir: path.join(tmpdir(), "vendor") }),
      /HTTP 404/,
    );
  } finally {
    stub.restore();
  }
});
