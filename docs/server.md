# Server

The server is a single binary, `tunnelpp-server`, usually deployed with Docker behind a reverse proxy that
terminates TLS.

## Configuration

All configuration is done with environment variables:

| Variable | Default | Description |
| -------- | ------- | ----------- |
| `TUNNELPP_TOKEN` | **required** | shared secret clients must present in `register` |
| `TUNNELPP_DOMAIN` | `tunnelpp.local` | base domain; tunnels are served on `<subdomain>.<domain>` |
| `TUNNELPP_HOST` | `0.0.0.0` | bind address of the listener |
| `TUNNELPP_HTTP_PORT` | `8080` | public HTTP/WebSocket port |
| `TUNNELPP_PUBLIC_SCHEME` | `http` | scheme used in the public URL sent to clients (`http` or `https`) |
| `TUNNELPP_REQUEST_TIMEOUT` | `30` | seconds to wait for the client's response before answering `504` |

The server refuses to start when `TUNNELPP_TOKEN` is missing or when a port is invalid.

## Docker

```sh
docker run -p 8080:8080 \
    -e TUNNELPP_DOMAIN=tunnel.example.com \
    -e TUNNELPP_TOKEN=change-me \
    ghcr.io/theo-dep/tunnelpp:latest
```

Or with the provided compose file:

```sh
TUNNELPP_TOKEN=change-me TUNNELPP_DOMAIN=tunnel.example.com docker compose up --build
```

Build the image yourself:

```sh
docker build -f server/Dockerfile -t tunnelpp-server .
```

## Reverse proxy

The server speaks plain HTTP. Put a reverse proxy in front of it to terminate TLS and route both the apex
domain and all subdomains to it. The proxy must preserve the `Host` header (the server routes on it).

nginx:

```nginx
server {
    listen 443 ssl;
    server_name tunnel.example.com *.tunnel.example.com;

    location / {
        proxy_pass http://127.0.0.1:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-Proto https;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
    }
}
```

Caddy:

```caddy
tunnel.example.com, *.tunnel.example.com {
    reverse_proxy 127.0.0.1:8080
}
```

With the proxy above, set `TUNNELPP_PUBLIC_SCHEME=https` so the URLs reported to clients use `https`.
