# Client

The client is a single binary, `tunnelpp-client`, available as prebuilt binaries (GitHub releases) and as the
npm package `@tunnelpp/client`.

## CLI

```
tunnelpp-client --server <ws:// or wss:// URL> --token <token> [options]
```

| Option | Default | Description |
| ------ | ------- | ----------- |
| `--server` | **required** | tunnel server WebSocket URL, e.g. `wss://tunnel.example.com/tunnel` |
| `--token` | **required** | authentication token shared with the server |
| `--local` | `http://127.0.0.1:8080` | local service URL to expose (`http://` or `https://`) |
| `--subdomain` | random | requested subdomain (`<subdomain>.<domain>`) |
| `--max-reconnect-delay` | `30` | maximum delay between reconnection attempts, in seconds |
| `--verbose` | off | debug logging |

Example:

```sh
tunnelpp-client \
    --server wss://tunnel.example.com/tunnel \
    --token change-me \
    --local http://127.0.0.1:3000 \
    --subdomain my-app
# tunnel ready: https://my-app.tunnel.example.com -> http://127.0.0.1:3000
```

The client reconnects automatically with an exponential backoff (1s, doubling up to
`--max-reconnect-delay`). It stops on `SIGINT`/`SIGTERM`, and exits with a non-zero code on fatal errors
(invalid token, requested subdomain taken, unsupported protocol version).

## npm package

```sh
npm install -g @tunnelpp/client
tunnelpp-client --server wss://tunnel.example.com/tunnel --token change-me --local http://127.0.0.1:3000
```

At install time, the package downloads the prebuilt binary for the current platform from the GitHub
releases (`tunnelpp-client-<platform>-<arch>.tar.gz`, `.zip` on Windows). Supported platforms: linux (x64,
arm64), macOS (x64, arm64), Windows (x64). The download location can be overridden with the `TUNNELPP_REPO`
environment variable (`<owner>/<repo>`).

### JS API

```js
const { startTunnel } = require('@tunnelpp/client');

const child = startTunnel({
    server: 'wss://tunnel.example.com/tunnel',
    token: 'change-me',
    local: 'http://127.0.0.1:3000',
    // subdomain: 'my-app',          // optional, random when omitted
    // maxReconnectDelay: 30,        // optional, seconds
    // verbose: true,                // optional
});

child.on('exit', (code) => console.log(`tunnelpp-client exited with ${code}`));
```

`startTunnel` spawns the binary and returns the child process; the assigned public URL is logged on stdout
by the client.

## Prebuilt binaries

Download a binary directly from the
[releases](https://github.com/tunnelpp/tunnelpp/releases) page: `tunnelpp-client-<platform>-<arch>.tar.gz`
(`.zip` on Windows).
