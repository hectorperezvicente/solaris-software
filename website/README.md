# Solaris Web

Static website for the Solaris project, served by an nginx container on a remote VPS and exposed publicly via a **Cloudflare Tunnel** at [softwaresolaris.com](https://softwaresolaris.com).

## How requests reach the browser

```
  User
  │
  │  https://softwaresolaris.com
  ▼
┌─────────────────────────────────┐
│         Cloudflare Edge         │  ← DNS, TLS termination, DDoS protection
└────────────────┬────────────────┘
                 │
                 │  Cloudflare Tunnel
                 │  (outbound connection — no open inbound ports on the VPS)
                 │
  ───────────────┼─── VPS boundary ───────────────────────────────
                 │
                 ▼
          cloudflared daemon
                 │
                 │  forwards to localhost:9173
                 ▼
    ┌────────────────────────┐
    │  Container: solaris-web │
    │  Image: nginx:alpine    │
    │  Port: 9173 → 80        │
    └────────────┬────────────┘
                 │
                 │  serves files from mounted html/ volume
                 ▼
       index.html · logo.svg · banner.png
```

**Key point:** the VPS does not expose any public HTTP/HTTPS port. The Cloudflare Tunnel (`cloudflared`) runs on the VPS and maintains a persistent outbound connection to Cloudflare's edge. All traffic flows through that tunnel — Cloudflare handles TLS, caching and protection before anything reaches the VPS.

## Structure

```
website/
├── docker-compose.yml   # Container definition (nginx + restart: always)
├── nginx.conf           # nginx config: gzip, cache headers, CSP
└── html/
    ├── index.html           # Main page
    ├── logo.svg             # Solaris logo
    ├── banner.png           # Banner image
    └── bannerPixelart.png   # Pixel-art banner variant
```

## What it is

A single-page static site (`index.html` + assets). No backend, no build step — editing the HTML files is all it takes to update the site.

The container is managed with **Podman** on the VPS (the `docker` and `docker-compose` commands are aliased to `podman` / `podman compose` on that machine). `restart: always` ensures the container comes back up automatically after a reboot.

The live files live at `~/solaris-web/` on the VPS. This `website/` folder in the repo is the source of truth.

## Managing the container

```bash
ssh raspi
cd ~/solaris-web

docker compose up -d      # start (or restart after a config change)
docker compose down       # stop
docker compose logs -f    # follow logs
```

## Updating the site

1. Edit the files under `html/` in this repo.
2. Copy them to the VPS:

```bash
scp website/html/* raspi:~/solaris-web/html/
```

The nginx container mounts `html/` as a read-only volume, so changes are live immediately — no container restart needed for HTML or asset updates. Only `nginx.conf` or `docker-compose.yml` changes require a `docker compose up -d`.
