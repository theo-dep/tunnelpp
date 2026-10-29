#!/usr/bin/env node
"use strict";

const { spawnSync } = require("child_process");
const fs = require("fs");
const path = require("path");

const binary = path.join(
  __dirname,
  "..",
  "vendor",
  process.platform === "win32" ? "tunnelpp-client.exe" : "tunnelpp-client",
);
if (!fs.existsSync(binary)) {
  console.error(
    "@theo-dep/tunnelpp: binary not found. Run `npm install` again, or download it manually from https://github.com/tunnelpp/tunnelpp/releases",
  );
  process.exit(1);
}
const result = spawnSync(binary, process.argv.slice(2), { stdio: "inherit" });
process.exit(result.status === null ? 1 : result.status);
