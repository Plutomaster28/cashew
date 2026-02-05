# Testing Cashew Locally

quick guide to test if the whole framework works.

## what you need running

1. **Cashew node** (port 8080) - the backend
2. **Perl CGI gateway** (port 8081) - the frontend
3. **A browser** - to see if it works

---

## step 1: start the cashew node

```bash
# in cashew root directory
./build/src/cashew_node.exe   # windows
# or
./build/src/cashew_node        # linux/macos
```

should see:
```
Cashew node is running!
Gateway:    http://localhost:8080
WebSocket:  ws://localhost:8080/ws
...
```

leave this running.

---

## step 2: test the node API

open another terminal:

```bash
# check if node is alive
curl http://localhost:8080/api/status

# should return something like:
# {"status": "ok", "node_id": "abc123...", "networks": 0}
```

if this doesn't work, node isn't running properly.

---

## step 3: start the perl CGI gateway

```bash
# in MSYS2 UCRT64 terminal (windows) or regular terminal (linux/mac)
cd examples/perl-cgi

# run the simple HTTP server
perl -MHTTP::Server::Simple::CGI -e 'package MyServer; use base qw(HTTP::Server::Simple::CGI); sub handle_request { my ($self, $cgi) = @_; do "cashew-gateway.pl"; } MyServer->new(8081)->run();'
```

or use the built-in CGI server:

```bash
perl -MCGI::HTTPServer -e 'my $server = CGI::HTTPServer->new(port => 8081); $server->run();'
```

**easier way (if you have python):**

```bash
# create a simple wrapper
python -m http.server 8081
```

---

## step 4: test the gateway

open browser to:
- http://localhost:8081/cashew-gateway.pl

should see the gateway home page with Mona.

---

## step 5: test content serving

**mock test (since we don't have upload API yet):**

the gateway can serve content if the node has it. for now:

1. **direct node access:**
   - http://localhost:8080/ - web UI (basic)
   
2. **gateway access:**
   - http://localhost:8081/cashew-gateway.pl - home
   - http://localhost:8081/cashew-gateway.pl/networks - list networks

---

## what you're testing

```
┌──────────┐         ┌──────────────┐         ┌─────────┐
│ Browser  │────────►│ Perl Gateway │────────►│  Node   │
│          │         │  (port 8081) │         │ (8080)  │
└──────────┘         └──────────────┘         └─────────┘
                            │                       │
                            │   HTTP requests       │
                            │   for Thing by hash   │
                            │                       │
                            └──────────────────────►│
                                                    │
                                              ┌─────▼─────┐
                                              │  Storage  │
                                              │  (hashed) │
                                              └───────────┘
```

---

## uploading the test site (manual)

since the upload API isn't exposed yet, you can test by:

1. **directly adding to node storage:**
   ```bash
   # copy test-site files to node's data directory
   # node will hash and serve them
   # (this is a hack for testing)
   ```

2. **wait for upload endpoint:**
   - POST to /api/thing/upload with multipart form
   - node hashes content, stores it
   - returns Thing ID
   - access via gateway: /thing/<hash>

---

## expected behavior

**if everything works:**
- node starts without errors
- API responds to /api/status
- gateway connects to node API
- gateway serves home page
- you can navigate gateway routes

**if it doesn't work:**

- **node won't start:** check dependencies (libsodium, spdlog)
- **port 8080 in use:** change config or kill other process
- **gateway errors:** check perl modules installed
- **can't connect:** firewall blocking localhost?

---

## testing checklist

- [ ] cashew node starts and runs
- [ ] curl http://localhost:8080/api/status returns JSON
- [ ] perl gateway starts without errors
- [ ] http://localhost:8081/cashew-gateway.pl loads
- [ ] gateway shows Mona and home page
- [ ] /networks route works (even if empty)

---

## next steps (when upload works)

1. upload test-site/ via API
2. get Thing ID (hash) back
3. access via: http://localhost:8081/thing/<hash>
4. see your website served through P2P storage
5. invite friends to network
6. they download and host it too
7. you go offline, they keep serving

---

## debugging

**node logs:**
```bash
# increase verbosity in cashew.conf
log_level = debug
```

**perl errors:**
```bash
# run gateway with:
perl -w cashew-gateway.pl
# shows warnings
```

**network check:**
```bash
# windows
netstat -ano | findstr :8080
netstat -ano | findstr :8081

# linux/mac
lsof -i :8080
lsof -i :8081
```

---

## current limitations

- no upload API endpoint yet (phase 7)
- web UI is basic
- can't create networks from browser
- manual testing only
- proper integration tests coming

but the core framework compiles and the node runs, so that's something.

---

last updated: february 1, 2026  
status: basic testing works, upload API todo
