# tunnelpp

tunnelpp exposes a local HTTP service on a public subdomain through a tunnel server. The client opens an
outbound WebSocket connection to the server and forwards incoming requests to the local service. No inbound
port, no DNS record, no certificate to manage on the client side.

```
public request -> tunnelpp-server (sub.domain.tld) -> WebSocket -> tunnelpp-client -> local service
```

## Components

| Component | Distribution | Role |
| --------- | ------------ | ---- |
| `tunnelpp-server` | Docker image (`ghcr.io/tunnelpp/server`) | Public HTTP listener, tunnel gateway, routing, admin API |
| `tunnelpp-client` | Binary releases + npm package (`@tunnelpp/client`) | Connects to the server, forwards traffic to a local service |

## Quickstart

Run the server (it expects to sit behind a reverse proxy that terminates TLS):

```sh
docker run -p 8080:8080 -p 127.0.0.1:8081:8081 \
    -e TUNNELPP_DOMAIN=tunnel.example.com \
    -e TUNNELPP_TOKEN=change-me \
    ghcr.io/tunnelpp/server:latest
```

Expose a local service:

```sh
npm install -g @tunnelpp/client
tunnelpp-client --server ws://tunnel.example.com/tunnel --token change-me --local http://127.0.0.1:3000
# tunnel ready: https://my-app.tunnel.example.com -> http://127.0.0.1:3000
```

## Next steps

- [Overview](overview.md) — concepts and the life of a request
- [Server](server.md) — configuration and deployment
- [Client](client.md) — CLI and npm package
- [Development](development.md) — build, test and contribute
