# Architecture

## Components

```
                        ┌──────────────────────────────┐
                        │         tunnelpp-server       │
                        │  ┌─────────┐   ┌───────────┐ │
        public HTTP ───▶│  │ public  │   │  admin    │ │──▶ admin API (localhost only)
        (via reverse     │  │ listener│   │  listener │ │
         proxy, TLS)     │  │  :8080  │   │  :8081    │ │
                        │  │  + WS   │   └───────────┘ │
                        │  │  /tunnel│                 │
                        │  └────┬────┘                 │
                        │  router + sessions + pending │
                        └───────┼──────────────────────┘
                                │ WebSocket (outbound from the client)
                        ┌───────┼──────────────────────┐
                        │       ▼                      │
                        │  ┌────────────────────┐      │
                        │  │   tunnelpp-client   │      │
                        │  │  register + backoff │      │
                        │  │  worker per request │      │
                        │  └─────────┬──────────┘      │
                        │            │ HTTP             │
                        │            ▼                  │
                        │     local service :3000       │
                        └───────────────────────────────┘
```

### Server (`server/`, library `server_core`)

- `tunnel_server.*`: `TunnelServer` — owns the two `httplib::Server` instances (public, admin), routes
  public requests to tunnels, runs the WebSocket gateway.
- `session.*`: `Session` (one connected tunnel: subdomain, URL, WebSocket, stats) and `SessionManager`
  (subdomain registry).
- `pending.*`: `PendingRequestTable` — public requests waiting for the client's `response` message.
- `router.*`: `Host` header to subdomain extraction.

### Client (`client/`, library `client_core`)

- `tunnel_client.*`: `TunnelClient` — WebSocket connection, registration, reconnection with exponential
  backoff (1s doubling up to `--max-reconnect-delay`), one worker thread per incoming request.
- `local_forwarder.*`: `forward_to_local` — forwards one tunnel request to the local HTTP service.
- `args.*`: CLI definition (argparse) and validation.

### Common (`common/`, library `common`)

Shared by both: the protocol (`protocol.*`), base64, UUIDs, subdomain validation/generation, URL parsing,
header filtering, server configuration from the environment, logging setup.

## Threading model

- Each `httplib::Server` runs its own thread pool. A WebSocket tunnel occupies one pool thread for its whole
  lifetime (the gateway runs a read loop); each concurrent public request occupies another.
- The client spawns one detached worker thread per incoming `request` message. `WebSocket::send` is
  internally serialized, so concurrent workers can share the connection safely.
- The server correlates requests and responses by `id` through `PendingRequestTable` (condition variable
  per pending request, with a timeout).

## Concurrency and limits

- One subdomain maps to exactly one connected client; a second registration on the same subdomain is
  rejected with `subdomain_taken`.
- WebSocket frames are limited to 64 MB (`CPPHTTPLIB_WEBSOCKET_MAX_PAYLOAD_LENGTH`), which bounds request
  and response bodies carried in a single message.
- WebSocket keep-alive: ping every 15s, 3 missed pongs before the connection is considered dead.
