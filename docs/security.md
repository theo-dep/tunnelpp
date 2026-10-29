# Security

## Threat model

- The **server** is public: anyone can send HTTP requests to it and open WebSocket connections to `/tunnel`.
- The **client** runs next to a local service that is usually not meant to be public.
- The **token** (`TUNNELPP_TOKEN`) is the only credential: it authorizes registering a tunnel. Anyone who has
  it can expose their own local service on the server's domain.
- The tunnel carries full HTTP requests and responses, including headers (cookies, authorization headers).

## Recommendations

- **TLS**: always put the server behind a reverse proxy that terminates TLS. Never expose the public port
  unencrypted: requests, responses and tunnel registrations (including the token) would be visible on the
  network.
- **Token**: use a long random value (`openssl rand -hex 32`), store it in a secret manager, rotate it on
  suspicion of leak. The token is sent by the client on every (re)connection.
- **Admin API**: bind `TUNNELPP_ADMIN_PORT` to localhost or an internal network, and set
  `TUNNELPP_ADMIN_TOKEN` so the API requires `Authorization: Bearer <token>`. The admin API can list and
  close tunnels.
- **Subdomains**: any client with the token can register any free subdomain. Treat subdomains as public
  URLs: the local service behind a tunnel is reachable by anyone who knows the URL. Random subdomains (10
  lowercase alphanumeric characters) are unguessable in practice; user-requested subdomains are not.
- **Local service**: the client forwards the request path, headers and body as received. If the local
  service has its own authentication, keep it: the tunnel does not add any.

## What the protocol protects

- Registration requires the token; a wrong token is rejected with `invalid_token` and the connection is
  closed.
- The first message on a tunnel connection must be `register` (10s deadline).
- Subdomain format is validated server-side.
- WebSocket keep-alive (ping every 15s, 3 missed pongs) detects dead connections; pending requests on a
  disconnected tunnel fail with `502`.

## Known limitations (v1)

- One shared token for all clients: there is no per-user or per-tunnel credential.
- No rate limiting on public requests or registration attempts.
- The admin API has no authentication unless `TUNNELPP_ADMIN_TOKEN` is set.
- Request/response bodies up to 64 MB are carried in a single WebSocket message (no streaming).

See the [roadmap](roadmap.md) for planned improvements.
