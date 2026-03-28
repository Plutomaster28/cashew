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
