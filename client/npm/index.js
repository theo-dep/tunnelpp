'use strict';

const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');

function binaryPath() {
    return path.join(__dirname, 'vendor', process.platform === 'win32' ? 'tunnelpp-client.exe' : 'tunnelpp-client');
}

// Starts a tunnel. options: { server, token, local?, subdomain?, maxReconnectDelay?, verbose? }
// Returns the child process; the assigned public URL is logged on stdout by the client.
function startTunnel(options) {
    if (!options || !options.server || !options.token) {
        throw new Error('startTunnel requires options.server and options.token');
    }
    if (!fs.existsSync(binaryPath())) {
        throw new Error('tunnelpp-client binary not found. Run `npm install` again.');
    }
    const args = ['--server', options.server, '--token', options.token];
    args.push('--local', options.local || 'http://127.0.0.1:8080');
    if (options.subdomain) {
        args.push('--subdomain', options.subdomain);
    }
    if (options.maxReconnectDelay) {
        args.push('--max-reconnect-delay', String(options.maxReconnectDelay));
    }
    if (options.verbose) {
        args.push('--verbose');
    }
    return spawn(binaryPath(), args, { stdio: 'inherit' });
}

module.exports = { startTunnel };
