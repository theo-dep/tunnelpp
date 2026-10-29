# @tunnelpp/client

npm distribution of the tunnelpp client: expose a local HTTP service on a public subdomain through a
tunnelpp server.

The package downloads the prebuilt `tunnelpp-client` binary for your platform at install time (from the
GitHub releases of this repository) and exposes it as a CLI and a small JS API.

## CLI

```sh
npm install -g @tunnelpp/client
tunnelpp-client --server ws://tunnel.example.com/tunnel --token change-me --local http://127.0.0.1:3000
```

## JS API

```js
const { startTunnel } = require('@tunnelpp/client');

const child = startTunnel({
    server: 'ws://tunnel.example.com/tunnel',
    token: 'change-me',
    local: 'http://127.0.0.1:3000',
    // subdomain: 'my-app',        // optional, random when omitted
});
```

See the [client documentation](https://tunnelpp.github.io/tunnelpp/client/) for all options.

## Supported platforms

linux (x64, arm64), macOS (x64, arm64), Windows (x64).
