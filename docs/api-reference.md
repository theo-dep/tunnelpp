# API reference

## Admin API

Served on `TUNNELPP_ADMIN_PORT` (default `8081`). When `TUNNELPP_ADMIN_TOKEN` is set, every endpoint
requires the header `Authorization: Bearer <TUNNELPP_ADMIN_TOKEN>`.

### `GET /healthz`

Liveness probe.

```json
{ "status": "ok" }
```

### `GET /api/v1/stats`

```json
{
    "active_sessions": 1,
    "total_sessions": 3,
    "total_requests": 42,
    "bytes_in": 1024,
    "bytes_out": 8192,
    "rejected_registers": 0,
    "uptime_seconds": 3600
}
```

### `GET /api/v1/sessions`

```json
{
    "sessions": [
        {
            "subdomain": "my-app",
            "url": "http://my-app.tunnel.example.com:8080",
            "connected_at": 1767225600,
            "requests": 12,
            "bytes_in": 512,
            "bytes_out": 4096
        }
    ]
}
```

`connected_at` is a Unix timestamp (seconds).

### `DELETE /api/v1/sessions/{subdomain}`

Closes the tunnel: the client's WebSocket is closed and the client reconnects (with a new subdomain when the
subdomain was auto-generated). Pending requests on the tunnel fail with `502`.

- `200` `{"closed":true}` — the tunnel was found and closed
- `404` `{"error":"session_not_found"}` — no tunnel on this subdomain

### Public listener errors

| Status | Body | Cause |
| ------ | ---- | ----- |
| `404` | `{"error":"tunnel_not_found",...}` | no tunnel registered on the requested subdomain |
| `502` | `{"error":"bad_gateway",...}` | tunnel disconnected mid-request, or the local service is unreachable |
| `504` | `{"error":"gateway_timeout",...}` | the client did not answer within `TUNNELPP_REQUEST_TIMEOUT` |

## Tunnel protocol (WebSocket)

See [Protocol](protocol.md) for the full message reference. Summary:

| Type | Direction | Purpose |
| ---- | --------- | ------- |
| `register` | client → server | register a tunnel on a subdomain (first message) |
| `registered` | server → client | registration accepted, carries the public URL |
| `request` | server → client | an incoming public HTTP request to forward |
| `response` | client → server | the local service's response, correlated by `id` |
| `error` | both | an error (`invalid_token`, `subdomain_taken`, `protocol_error`, `bad_message`, ...) |
| `ping` / `pong` | both | application-level keep-alive |
