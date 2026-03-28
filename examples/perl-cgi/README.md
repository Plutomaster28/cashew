# Miyamii Guestbook CGI Demo

This demo serves the site from `test-site/miyamii` with a working Perl CGI guestbook backend.

## Requirements

- Perl 5.10+
- `HTTP::Server::Simple::CGI`
- `JSON::PP` (core on most Perl installs)

Install server module if needed:

```powershell
cpanm HTTP::Server::Simple::CGI
```

## Run

From repository root:

```powershell
perl .\examples\perl-cgi\server.pl
```

Then open:

- `http://localhost:8090/miyamii/`

Optional custom port:

```powershell
perl .\examples\perl-cgi\server.pl 8085
```

## Notes

- Posts are persisted to `test-site/miyamii/data/guestbook.json`.
- The static Cashew gateway also serves this site from `test-site` (default web root), but static-only mode cannot execute CGI.
- In static-only mode, the page automatically falls back to localStorage posting.

## End-to-End Dynamic Test

Run both processes in separate terminals from repo root:

```powershell
# Terminal 1: Cashew static gateway
.\build\src\cashew.exe

# Terminal 2: Perl guestbook backend
perl .\examples\perl-cgi\server.pl 8000
```

Then verify:

- Site: `http://localhost:8080/miyamii/`
- Guestbook API: `http://localhost:8000/miyamii/cgi-bin/guestbook.pl?action=list`

Quick POST test:

```powershell
$body = '{"name":"smoke-test","subject":"dynamic","body":"hello from powershell"}'
Invoke-WebRequest -UseBasicParsing `
	-Uri "http://localhost:8000/miyamii/cgi-bin/guestbook.pl" `
	-Method POST `
	-ContentType "application/json" `
	-Body $body
```

## Public Access + Custom Domain (Caddy)

Use Caddy as reverse proxy for HTTPS + custom URL.

1. Set DNS `A` record for your domain to your public IP.
2. Forward ports `80` and `443` on your router to this machine.
3. Allow `80/443` through Windows Firewall.
4. Copy `examples/perl-cgi/Caddyfile.example` to `Caddyfile` and set your domain.
5. Run Caddy with that file.

Routing model:

- `https://your-domain/` -> Cashew (`127.0.0.1:8080`)
- `https://your-domain/miyamii/cgi-bin/guestbook.pl` -> Perl CGI (`127.0.0.1:8000`)

This gives you a single public URL with dynamic guestbook support.
