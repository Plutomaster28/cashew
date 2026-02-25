# Cashew Network - Full Featured Perl CGI Demo

This comprehensive demo showcases all major features of the Cashew decentralized framework v1.0.

## Features Demonstrated

### 🗂️ Content System (Things)
- Publish content up to 500MB
- Content-addressed storage using BLAKE3 hashing
- Metadata support
- File upload capability
- Content deduplication
- Browse and retrieve published Things

### 🔐 Identity Management
- Create Node and Human identities
- Ed25519 key generation and management
- Key rotation with certificate chains
- Identity verification
- Secure key storage (encrypted at rest)

### 🔑 Participation Keys
- 5 key types: Identity, Node, Network, Service, Routing
- PoW-gated key issuance
- Time-based decay (exponential)
- 10-minute epoch system
- Key transfer and vouching
- Per-epoch issuance limits

### ⚡ Proof-of-Work
- Argon2id memory-hard puzzles
- Adaptive difficulty calculation
- Network entropy incorporation
- Node benchmarking
- Solution verification
- Anti-spam protection

### ⭐ Reputation System
- Trust attestations between nodes
- Reputation scoring
- Vouching workflow
- Trust path finding
- Bridge node detection
- Top nodes leaderboard

### 📊 Ledger & State
- Blockchain-based ledger
- State reconciliation
- Entry types: thing_publish, key_issue, attestation, transfer
- Ledger history tracking
- Hash chain verification

### 🌐 Network Features
- Network status monitoring
- Peer list management
- Connectivity stats
- Latency tracking
- Version information

### 📈 Statistics & Analytics
- Real-time metrics
- Content statistics
- Identity counts
- Key issuance tracking
- Performance metrics

## Requirements

- Perl 5.10 or higher
- CGI module
- JSON::PP module
- HTTP::Server::Simple::CGI module

Install required modules:
```bash
cpan CGI JSON::PP HTTP::Server::Simple::CGI
```

Or on Windows with Strawberry Perl:
```bash
cpanm CGI JSON::PP HTTP::Server::Simple::CGI
```

## Running the Demo

1. Start the server:
```bash
perl server.pl
```

2. Open your browser to:
```
http://localhost:8000
```

3. The demo will be fully interactive with all Cashew features available

## API Endpoints

All endpoints are available via the Perl CGI gateway:

- **POST /api/thing** - Publish content
- **GET /api/thing** - List/retrieve Things
- **POST /api/identity** - Create identity
- **GET /api/identity** - List identities
- **PUT /api/identity?action=rotate_key** - Rotate keys
- **POST /api/keys** - Request participation key
- **GET /api/keys** - List keys
- **PUT /api/keys?action=transfer** - Transfer key
- **PUT /api/keys?action=vouch** - Vouch for key
- **GET /api/pow** - Get PoW puzzle
- **POST /api/pow** - Submit PoW solution
- **POST /api/reputation** - Submit attestation
- **GET /api/reputation** - Get reputation data
- **GET /api/ledger** - Get ledger state
- **POST /api/ledger** - Add ledger entry
- **GET /api/network** - Network status
- **GET /api/search** - Search content
- **GET /api/stats** - System statistics
- **POST /api/vouch** - Create vouch record
- **GET /api/vouch** - List vouches

## Architecture

```
perl-full-demo/
├── server.pl           # HTTP server with CGI support
├── cashew-api.pl       # Complete API gateway
├── index.html          # Interactive web interface
├── data/               # Data storage (created automatically)
│   ├── thing_*.json
│   ├── identity_*.json
│   ├── key_*.json
│   ├── attestation_*.json
│   └── ledger_state.json
└── README.md           # This file
```

## Example Usage

### Publishing Content
```javascript
// Via JavaScript in browser
const response = await fetch('/api/thing', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
        content: 'Hello Cashew Network!',
        metadata: '{"title": "First Post", "author": "Alice"}'
    })
});
```

### Creating Identity
```javascript
const response = await fetch('/api/identity', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
        type: 'node',
        name: 'My Cashew Node'
    })
});
```

### Requesting Participation Key
```javascript
const response = await fetch('/api/keys', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
        node_id: 'abc123...',
        type: 'Network',
        pow_solution: 'valid_solution_hash'
    })
});
```

## Integration with Cashew Node

This demo can be integrated with a running Cashew node by setting:

```bash
export CASHEW_NODE_URL=http://localhost:8080
perl server.pl
```

The CGI gateway will proxy requests to the actual Cashew node implementation.

## Security Notes

This is a demonstration implementation. In production:
- Private keys must be properly encrypted
- PoW verification should use actual Argon2id
- HTTPS should be enforced
- Rate limiting should be implemented
- Input validation should be comprehensive
- CSRF protection should be added

## License

Same as Cashew Network (see main LICENSE file)

## Contributing

This demo showcases the complete Cashew v1.0 framework. For contributions to the core
Cashew implementation, see the main project repository.
