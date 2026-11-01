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

https://tunnel.example.com   (the WebSocket endpoint clients connect to)
  → DNS: tunnel.example.com CNAME dyn.tunnel.example.com
  → same chain
```

When the public IP changes, the DynHost client updates `dyn.tunnel.example.com` and every tunnel subdomain follows through the CNAME. One DynHost entry to maintain.

## DNS zone (OVH manager, text mode)

```
dyn.tunnel.example.com.    300    IN    A        203.0.113.10
*.tunnel.example.com.      300    IN    CNAME    dyn.tunnel.example.com.
tunnel.example.com.        300    IN    CNAME    dyn.tunnel.example.com.
```

Notes:

- The trailing dot after `dyn.tunnel.example.com` is required (FQDN target).
- TTL 300 (5 min) so IP changes propagate quickly through the CNAME chain.
- `tunnel.example.com` is a regular subdomain of the zone, so it can be a CNAME (a CNAME is only forbidden at the root of a zone).
- The bare `tunnel.example.com` needs its own record: a wildcard never covers the name itself (`*.tunnel.example.com` matches `my-app.tunnel.example.com`, never `tunnel.example.com`).
- Do not rely on an outer wildcard of the parent domain (e.g. `*.example.com`) to resolve the bare name: every DNS-01 validation of the
  `*.tunnel.example.com` certificate creates a TXT record at `_acme-challenge.tunnel.example.com`, and that child node makes
  `tunnel.example.com` an existing (empty) name — wildcard synthesis stops applying to it (RFC 4592), and the WebSocket endpoint becomes
  unreachable for as long as the challenge record exists. The explicit CNAME makes the name independent of wildcards.
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

No entry for the bare `tunnel.example.com`: it follows `dyn.tunnel.example.com` through its CNAME.

## Server and reverse proxy

The box forwards ports 80/443 to the host running nginx. The server and the proxy see the original `Host` header, so nothing changes:

```sh
docker run -p 8080:8080 \
    -e TUNNELPP_DOMAIN=tunnel.example.com \
    -e TUNNELPP_TOKEN=change-me \
    -e TUNNELPP_PUBLIC_SCHEME=https \
    -e TUNNELPP_PUBLIC_PORT=443 \
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

## Troubleshooting

DNS and certificate failures around the DNS-01 wildcard, and how to tell them apart.

### The client cannot connect to `wss://tunnel.example.com`

```sh
dig tunnel.example.com A +noall +comments          # status: NOERROR, empty answer
dig _acme-challenge.tunnel.example.com TXT         # a TXT record is still there
```

`NOERROR` with an empty answer means the name **exists** but carries no data: a leftover `_acme-challenge` TXT (a failed or interrupted certbot run leaves it behind) makes `tunnel.example.com` an
existing empty node, which disables wildcard synthesis for it (RFC 4592). This is the symptom of relying on an outer wildcard instead of the explicit `tunnel.example.com` CNAME. The leftover TXT is
harmless and can be deleted once certbot is done — with the explicit CNAME in place, the endpoint stays reachable even while the TXT exists.

### Tunnel subdomains do not resolve

```sh
dig my-app.tunnel.example.com A +noall +comments   # status: NXDOMAIN
dig '*.tunnel.example.com' CNAME +noall +answer     # no answer
```

`NXDOMAIN` means the name does not exist at all: the `*.tunnel.example.com` record is missing from the zone (add it in text mode, see above). `NXDOMAIN` on `*.tunnel.example.com` itself confirms it.

### The browser still reports an insecure connection

Check what certificate nginx actually serves, and what SANs the lineage has:

```sh
echo | openssl s_client -connect tunnel.example.com:443 -servername my-app.tunnel.example.com 2>/dev/null \
    | openssl x509 -noout -ext subjectAltName
sudo certbot certificates
```

- The lineage does not list `*.tunnel.example.com`: the issuance did not include it. Re-run `certbot certonly` with the full `-d` list; `--cert-name` **replaces** the SAN set with the given `-d` names.
- The lineage lists it but nginx still serves the old certificate: the renewed `fullchain.pem`/`privkey.pem` were not copied to the directory nginx mounts, or nginx was not reloaded.

Automate the copy and reload with a deploy hook, so renewals deploy themselves (also runs on every `certbot renew`):

```sh
sudo certbot certonly \
    --cert-name tunnel.example.com \
    -d tunnel.example.com -d '*.tunnel.example.com' \
    --dns-ovh --dns-ovh-credentials /path/to/ovh-credentials \
    --deploy-hook 'cp /etc/letsencrypt/live/tunnel.example.com/{fullchain.pem,privkey.pem} /path/to/nginx/certificates/tunnel.example.com/ \
        && docker exec nginx nginx -s reload'
```

Wildcard SANs require DNS-01 (`--dns-ovh`): HTTP-01 cannot issue wildcards. Use `--dry-run` to rehearse an issuance.

## Final setup

| Component      | Value                                                                                          |
| -------------- | ---------------------------------------------------------------------------------------------- |
| DNS zone       | `*.tunnel.example.com CNAME dyn.tunnel.example.com`, `tunnel.example.com CNAME dyn.tunnel.example.com`, `dyn.tunnel.example.com A <ip>` |
| DynHost        | 1 entry (`dyn.tunnel.example.com`)                                                             |
| DynHost client | 1 entry (`dyn.tunnel.example.com`)                                                            |
| Server env     | `TUNNELPP_DOMAIN=tunnel.example.com`, `TUNNELPP_PUBLIC_SCHEME=https`, `TUNNELPP_PUBLIC_PORT=443` |
