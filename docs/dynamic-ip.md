# Dynamic IP and wildcard DNS

Run `tunnelpp-server` at home, behind an ISP box with a dynamic public IP, on a custom domain: tunnels are served on `*.tunnel.example.com` and must follow the IP automatically.

## The problem

- The box has a dynamic public IP: the DNS records must be updated whenever it changes (DynDNS).
- Tunnels live on subdomains: `*.tunnel.example.com` needs a wildcard DNS record.
- OVH DynHost refuses wildcards:
  - the DNS zone web UI rejects `*` as a subdomain ("* is not a valid subdomain"); the record must be added in text mode;
  - the DynHost web UI rejects `*` as a field;
  - the DynHost update endpoint rejects wildcard hostnames:

    ```
    GET https://www.ovh.com/nic/update?system=dyndns&hostname=*.tunnel.example.com&myip=203.0.113.10
    → {"class":"Client::BadRequest","message":"{'hostname': ['Invalid domain name']}"}
    ```

So the DynHost client (the box) cannot update `*.tunnel.example.com` directly.

## The solution

One DynHost entry on a dedicated subdomain (`dyn.tunnel.example.com`) plus a wildcard CNAME pointing to it. A wildcard CNAME is valid DNS (RFC 4592) and is added in text mode, bypassing the UI restriction:

```
https://my-app.tunnel.example.com
  → DNS: *.tunnel.example.com CNAME dyn.tunnel.example.com
  → DNS: dyn.tunnel.example.com A 203.0.113.10   (the box public IP)
  → box ports 80/443 → nginx (TLS) → tunnelpp-server:8080
```

When the public IP changes, the DynHost client updates `dyn.tunnel.example.com` and every tunnel subdomain follows through the CNAME. One DynHost entry to maintain.

## DNS zone (OVH manager, text mode)

```
dyn.tunnel.example.com.    300    IN    A        203.0.113.10
*.tunnel.example.com.      300    IN    CNAME    dyn.tunnel.example.com.
tunnel.example.com.        300    IN    A        203.0.113.10
```

Notes:

- The trailing dot after `dyn.tunnel.example.com` is required (FQDN target).
- TTL 300 (5 min) so IP changes propagate quickly through the CNAME chain.
- The apex (`tunnel.example.com`) stays an A record with its own DynHost entry: a CNAME cannot exist at the root of a zone.
- Remove per-subdomain A records (e.g. `video.tunnel.example.com`) so they resolve through the wildcard CNAME instead of a stale static IP.

## DynHost (OVH manager)

`Domain` → `tunnel.example.com` → `DynHost` tab → `Add a DynHost`:

| Field    | Value                         |
| -------- | ----------------------------- |
| Field    | `dyn.tunnel.example.com`      |
| Login    | e.g. `tunnel.example.com-dyn` |
| Password | (generated, note it)          |

No wildcard anywhere, so the UI accepts it.

## DynHost client (ISP box / Livebox)

Replace any wildcard entry with:

| Field    | Value                           |
| -------- | ------------------------------- |
| Hostname | `dyn.tunnel.example.com`        |
| Username | the DynHost login created above |
| Password | the DynHost password            |

Keep one separate entry for the apex `tunnel.example.com` (a wildcard never covers the root domain).

## Server and reverse proxy

The box forwards ports 80/443 to the host running nginx. The server and the proxy see the original `Host` header, so nothing changes:

```sh
docker run -p 8080:8080 -p 127.0.0.1:8081:8081 \
    -e TUNNELPP_DOMAIN=tunnel.example.com \
    -e TUNNELPP_TOKEN=change-me \
    -e TUNNELPP_PUBLIC_SCHEME=https \
    ghcr.io/theo-dep/tunnelpp:latest
```

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

The wildcard certificate (`*.tunnel.example.com` + `tunnel.example.com`, Let's Encrypt DNS-01) keeps working unchanged: the CNAME is transparent for TLS — the browser resolves the CNAME, then connects to the IP using the original hostname (SNI), which the wildcard certificate covers.

## Verification

```sh
dig my-app.tunnel.example.com @dnsXXX.ovh.net +short   # → dyn.tunnel.example.com, then 203.0.113.10
dig video.tunnel.example.com  @dnsXXX.ovh.net +short   # → same, via the wildcard
curl -sI https://my-app.tunnel.example.com             # → valid wildcard cert, routed to the tunnel
```

To prove the dynamic update works: set the `dyn.tunnel.example.com` A record to a bogus IP in the zone (text mode), wait for the next DynHost client update (or reboot the box to force it), and check that `dig` returns the real public IP again.

## Final setup

| Component      | Value                                                                                          |
| -------------- | ---------------------------------------------------------------------------------------------- |
| DNS zone       | `*.tunnel.example.com CNAME dyn.tunnel.example.com`, `dyn.tunnel.example.com A <ip>`, `tunnel.example.com A <ip>` |
| DynHost        | 1 entry (`dyn.tunnel.example.com`)                                                             |
| DynHost client | 2 entries (`dyn.tunnel.example.com` + apex `tunnel.example.com`)                               |
| Server env     | `TUNNELPP_DOMAIN=tunnel.example.com`, `TUNNELPP_PUBLIC_SCHEME=https`                            |
