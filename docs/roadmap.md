# Roadmap

## v1 (current)

- WebSocket tunnel protocol v1 (JSON messages, base64 bodies, `request`/`response` correlation by `id`)
- Server: public listener, WebSocket gateway `/tunnel`, subdomain routing, token auth, admin API
  (`/healthz`, `/api/v1/stats`, `/api/v1/sessions`), Docker image
- Client: CLI (argparse), automatic subdomain, reconnection with exponential backoff, local HTTP/HTTPS
  forwarding, npm package with prebuilt binaries
- Tests: Catch2 unit tests + end-to-end suite, Debug + ASan/UBSan, clang coverage
- CI: Linux/Windows/macOS, Dependabot, releases, Docker, GitHub Pages docs

## v1.1

- Streaming of request/response bodies (binary WebSocket frames) instead of single base64 messages
- Optional per-tunnel tokens (one credential per tunnel instead of one global token)
- Prometheus metrics endpoint on the admin API
- Configurable limits (body size, concurrent requests per tunnel)

## v2

- Custom domains (bring your own domain, mapped to a tunnel)
- Load balancing: several clients on the same subdomain, requests spread across them
- Web dashboard (served by the server) to inspect tunnels and traffic
- TCP tunnels (not only HTTP)
- Hot-reload of the token, graceful drain of tunnels on shutdown
