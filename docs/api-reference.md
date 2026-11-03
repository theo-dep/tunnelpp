# API reference

## Public listener errors

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
