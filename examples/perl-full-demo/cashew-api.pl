#!/usr/bin/env perl
# Cashew API Gateway - Full-featured CGI interface
# Demonstrates all Cashew framework capabilities

use strict;
use warnings;
use CGI qw(:standard);
use JSON::PP;
use File::Spec;
use File::Basename;
use MIME::Base64;
use Digest::SHA qw(sha256_hex);
use Time::HiRes qw(time);

# Configuration
my $CASHEW_NODE_URL = $ENV{'CASHEW_NODE_URL'} || 'http://localhost:8080';
my $DATA_DIR = $ENV{'CASHEW_DATA_DIR'} || './data';
my $MAX_UPLOAD_SIZE = 500 * 1024 * 1024; # 500MB (Cashew limit)

# Ensure data directory exists
mkdir $DATA_DIR unless -d $DATA_DIR;

# Main entry point
main();

sub main {
    my $q = CGI->new;
    
    # Handle CORS for modern web apps
    print $q->header(
        -type => 'application/json',
        -charset => 'utf-8',
        -Access_Control_Allow_Origin => '*',
        -Access_Control_Allow_Methods => 'GET, POST, PUT, DELETE, OPTIONS',
        -Access_Control_Allow_Headers => 'Content-Type, Authorization'
    );
    
    # Get request method and path
    my $method = $ENV{'REQUEST_METHOD'} || 'GET';
    my $path = $ENV{'PATH_INFO'} || '/';
    
    # Route the request
    if ($method eq 'OPTIONS') {
        print encode_json({ status => 'ok' });
    } elsif ($path =~ m{^/api/thing/?$}) {
        handle_thing_api($q, $method);
    } elsif ($path =~ m{^/api/identity/?$}) {
        handle_identity_api($q, $method);
    } elsif ($path =~ m{^/api/keys/?$}) {
        handle_keys_api($q, $method);
    } elsif ($path =~ m{^/api/pow/?$}) {
        handle_pow_api($q, $method);
    } elsif ($path =~ m{^/api/reputation/?$}) {
        handle_reputation_api($q, $method);
    } elsif ($path =~ m{^/api/ledger/?$}) {
        handle_ledger_api($q, $method);
    } elsif ($path =~ m{^/api/network/?$}) {
        handle_network_api($q, $method);
    } elsif ($path =~ m{^/api/search/?$}) {
        handle_search_api($q, $method);
    } elsif ($path =~ m{^/api/stats/?$}) {
        handle_stats_api($q, $method);
    } elsif ($path =~ m{^/api/vouch/?$}) {
        handle_vouch_api($q, $method);
    } else {
        error_response(404, 'Not Found', "Unknown API endpoint: $path");
    }
}

# ============================================================================
# THING (Content) API - Core content publishing system
# ============================================================================

sub handle_thing_api {
    my ($q, $method) = @_;
    
    if ($method eq 'POST') {
        # Publish new Thing
        my $content = $q->param('content');
        my $metadata = $q->param('metadata') || '{}';
        my $file = $q->param('file');
        
        # Handle file upload
        my $thing_data;
        if ($file) {
            my $fh = $q->upload('file');
            if ($fh) {
                local $/;
                $thing_data = <$fh>;
                if (length($thing_data) > $MAX_UPLOAD_SIZE) {
                    error_response(413, 'Payload Too Large', 'File exceeds 500MB limit');
                    return;
                }
            }
        } elsif ($content) {
            $thing_data = $content;
        } else {
            error_response(400, 'Bad Request', 'No content or file provided');
            return;
        }
        
        # Create Thing via Cashew node
        my $thing = {
            content => encode_base64($thing_data, ''),
            metadata => decode_json($metadata),
            timestamp => time(),
            size => length($thing_data)
        };
        
        # Calculate content hash (BLAKE3 simulation)
        my $content_hash = sha256_hex($thing_data);
        $thing->{hash} = $content_hash;
        
        # Save to local cache
        my $thing_file = "$DATA_DIR/thing_$content_hash.json";
        open my $fh, '>', $thing_file or die "Cannot write thing: $!";
        print $fh encode_json($thing);
        close $fh;
        
        print encode_json({
            status => 'success',
            thing_id => $content_hash,
            size => length($thing_data),
            hash => $content_hash,
            url => "/api/thing/$content_hash"
        });
        
    } elsif ($method eq 'GET') {
        # Retrieve Thing
        my $thing_id = $q->param('id');
        
        if (!$thing_id) {
            # List all Things
            my @things;
            opendir my $dh, $DATA_DIR or die "Cannot open data dir: $!";
            while (my $file = readdir $dh) {
                next unless $file =~ /^thing_(.+)\.json$/;
                push @things, $1;
            }
            closedir $dh;
            
            print encode_json({
                status => 'success',
                things => \@things,
                count => scalar @things
            });
        } else {
            # Get specific Thing
            my $thing_file = "$DATA_DIR/thing_$thing_id.json";
            if (-f $thing_file) {
                open my $fh, '<', $thing_file or die "Cannot read thing: $!";
                local $/;
                my $thing = decode_json(<$fh>);
                close $fh;
                
                print encode_json({
                    status => 'success',
                    thing => $thing
                });
            } else {
                error_response(404, 'Not Found', 'Thing not found');
            }
        }
    } else {
        error_response(405, 'Method Not Allowed', "Method $method not supported");
    }
}

# ============================================================================
# IDENTITY API - Node and human identity management
# ============================================================================

sub handle_identity_api {
    my ($q, $method) = @_;
    
    if ($method eq 'POST') {
        # Create new identity
        my $identity_type = $q->param('type') || 'node';
        my $name = $q->param('name') || 'Anonymous';
        
        # Generate Ed25519 keypair (simulated)
        my $public_key = generate_random_hex(64);
        my $private_key = generate_random_hex(128);
        my $node_id = substr(sha256_hex($public_key), 0, 40);
        
        my $identity = {
            node_id => $node_id,
            type => $identity_type,
            name => $name,
            public_key => $public_key,
            created_at => time(),
            reputation_score => 0.0
        };
        
        # Save identity (private key encrypted in real implementation)
        my $identity_file = "$DATA_DIR/identity_$node_id.json";
        open my $fh, '>', $identity_file or die "Cannot write identity: $!";
        print $fh encode_json($identity);
        close $fh;
        
        print encode_json({
            status => 'success',
            identity => $identity,
            note => 'Private key secured (not returned in API)'
        });
        
    } elsif ($method eq 'GET') {
        my $node_id = $q->param('id');
        
        if (!$node_id) {
            # List identities
            my @identities;
            opendir my $dh, $DATA_DIR or die "Cannot open data dir: $!";
            while (my $file = readdir $dh) {
                next unless $file =~ /^identity_(.+)\.json$/;
                my $id = $1;
                open my $fh, '<', "$DATA_DIR/$file";
                local $/;
                my $identity = decode_json(<$fh>);
                close $fh;
                push @identities, $identity;
            }
            closedir $dh;
            
            print encode_json({
                status => 'success',
                identities => \@identities,
                count => scalar @identities
            });
        } else {
            # Get specific identity
            my $identity_file = "$DATA_DIR/identity_$node_id.json";
            if (-f $identity_file) {
                open my $fh, '<', $identity_file;
                local $/;
                my $identity = decode_json(<$fh>);
                close $fh;
                
                print encode_json({
                    status => 'success',
                    identity => $identity
                });
            } else {
                error_response(404, 'Not Found', 'Identity not found');
            }
        }
    } elsif ($method eq 'PUT') {
        # Update identity (rotation, etc.)
        my $node_id = $q->param('id');
        my $action = $q->param('action') || 'update';
        
        if ($action eq 'rotate_key') {
            # Key rotation
            my $identity_file = "$DATA_DIR/identity_$node_id.json";
            if (-f $identity_file) {
                open my $fh, '<', $identity_file;
                local $/;
                my $identity = decode_json(<$fh>);
                close $fh;
                
                # Generate new key
                my $new_public_key = generate_random_hex(64);
                my $old_public_key = $identity->{public_key};
                $identity->{public_key} = $new_public_key;
                $identity->{previous_keys} = [] unless exists $identity->{previous_keys};
                push @{$identity->{previous_keys}}, {
                    key => $old_public_key,
                    rotated_at => time()
                };
                
                # Save updated identity
                open $fh, '>', $identity_file;
                print $fh encode_json($identity);
                close $fh;
                
                print encode_json({
                    status => 'success',
                    message => 'Key rotated successfully',
                    new_key => $new_public_key
                });
            } else {
                error_response(404, 'Not Found', 'Identity not found');
            }
        }
    } else {
        error_response(405, 'Method Not Allowed', "Method $method not supported");
    }
}

# ============================================================================
# KEYS API - Participation key system (Identity, Node, Network, Service, Routing)
# ============================================================================

sub handle_keys_api {
    my ($q, $method) = @_;
    
    if ($method eq 'POST') {
        # Issue participation key
        my $node_id = $q->param('node_id') || error_response(400, 'Bad Request', 'node_id required');
        my $key_type = $q->param('type') || 'Network';
        my $pow_solution = $q->param('pow_solution');
        
        # Validate PoW solution (in real implementation)
        unless ($pow_solution) {
            error_response(403, 'Forbidden', 'Valid PoW solution required');
            return;
        }
        
        my $key_id = generate_random_hex(32);
        my $epoch = int(time() / 600); # 10-minute epochs
        
        my $key = {
            key_id => $key_id,
            node_id => $node_id,
            type => $key_type,
            epoch => $epoch,
            issued_at => time(),
            expires_at => time() + 600, # 10 minutes
            decay_rate => 0.1,
            current_power => 1.0
        };
        
        my $key_file = "$DATA_DIR/key_$key_id.json";
        open my $fh, '>', $key_file;
        print $fh encode_json($key);
        close $fh;
        
        print encode_json({
            status => 'success',
            key => $key
        });
        
    } elsif ($method eq 'GET') {
        my $node_id = $q->param('node_id');
        my $key_type = $q->param('type');
        
        # List keys for node
        my @keys;
        opendir my $dh, $DATA_DIR;
        while (my $file = readdir $dh) {
            next unless $file =~ /^key_(.+)\.json$/;
            open my $fh, '<', "$DATA_DIR/$file";
            local $/;
            my $key = decode_json(<$fh>);
            close $fh;
            
            # Filter by node_id and type if specified
            if ($node_id && $key->{node_id} ne $node_id) {
                next;
            }
            if ($key_type && $key->{type} ne $key_type) {
                next;
            }
            
            # Calculate current decay
            my $age = time() - $key->{issued_at};
            $key->{current_power} = exp(-$key->{decay_rate} * $age);
            
            push @keys, $key;
        }
        closedir $dh;
        
        print encode_json({
            status => 'success',
            keys => \@keys,
            count => scalar @keys
        });
        
    } elsif ($method eq 'PUT') {
        # Transfer or vouch key
        my $key_id = $q->param('key_id');
        my $action = $q->param('action');
        
        if ($action eq 'transfer') {
            my $to_node = $q->param('to_node');
            
            my $key_file = "$DATA_DIR/key_$key_id.json";
            if (-f $key_file) {
                open my $fh, '<', $key_file;
                local $/;
                my $key = decode_json(<$fh>);
                close $fh;
                
                $key->{node_id} = $to_node;
                $key->{transferred_at} = time();
                
                open $fh, '>', $key_file;
                print $fh encode_json($key);
                close $fh;
                
                print encode_json({
                    status => 'success',
                    message => 'Key transferred',
                    key => $key
                });
            } else {
                error_response(404, 'Not Found', 'Key not found');
            }
        } elsif ($action eq 'vouch') {
            my $vouchee = $q->param('vouchee');
            
            print encode_json({
                status => 'success',
                message => 'Vouch recorded',
                voucher => $q->param('node_id'),
                vouchee => $vouchee,
                timestamp => time()
            });
        }
    } else {
        error_response(405, 'Method Not Allowed', "Method $method not supported");
    }
}

# ============================================================================
# PROOF-OF-WORK API - Adaptive PoW system
# ============================================================================

sub handle_pow_api {
    my ($q, $method) = @_;
    
    if ($method eq 'GET') {
        # Get current PoW puzzle
        my $epoch = int(time() / 600);
        my $difficulty = calculate_difficulty();
        
        my $puzzle = {
            epoch => $epoch,
            difficulty => $difficulty,
            algorithm => 'Argon2id',
            parameters => {
                time_cost => 2,
                memory_cost => 65536,
                parallelism => 4
            },
            target => generate_random_hex(64),
            entropy => generate_random_hex(32)
        };
        
        print encode_json({
            status => 'success',
            puzzle => $puzzle
        });
        
    } elsif ($method eq 'POST') {
        # Submit PoW solution
        my $solution = $q->param('solution');
        my $nonce = $q->param('nonce');
        my $node_id = $q->param('node_id');
        
        # Verify solution (simplified)
        my $is_valid = verify_pow_solution($solution, $nonce);
        
        if ($is_valid) {
            print encode_json({
                status => 'success',
                valid => 1,
                message => 'PoW solution accepted',
                reward => {
                    key_issuance => 1,
                    reputation_boost => 0.1
                }
            });
        } else {
            print encode_json({
                status => 'fail',
                valid => 0,
                message => 'Invalid PoW solution'
            });
        }
    } else {
        error_response(405, 'Method Not Allowed', "Method $method not supported");
    }
}

# ============================================================================
# REPUTATION API - Trust and reputation system
# ============================================================================

sub handle_reputation_api {
    my ($q, $method) = @_;
    
    if ($method eq 'GET') {
        my $node_id = $q->param('node_id');
        
        if ($node_id) {
            # Get reputation for specific node
            my $reputation = get_node_reputation($node_id);
            
            print encode_json({
                status => 'success',
                node_id => $node_id,
                reputation => $reputation
            });
        } else {
            # Get top nodes by reputation
            my @top_nodes = get_top_reputation_nodes(10);
            
            print encode_json({
                status => 'success',
                top_nodes => \@top_nodes
            });
        }
        
    } elsif ($method eq 'POST') {
        # Submit attestation
        my $attester = $q->param('attester');
        my $attestee = $q->param('attestee');
        my $rating = $q->param('rating') || 5;
        my $comment = $q->param('comment') || '';
        
        my $attestation = {
            id => generate_random_hex(32),
            attester => $attester,
            attestee => $attestee,
            rating => $rating,
            comment => $comment,
            timestamp => time()
        };
        
        my $att_file = "$DATA_DIR/attestation_$attestation->{id}.json";
        open my $fh, '>', $att_file;
        print $fh encode_json($attestation);
        close $fh;
        
        # Update reputation
        update_reputation($attestee, $rating);
        
        print encode_json({
            status => 'success',
            attestation => $attestation
        });
    } else {
        error_response(405, 'Method Not Allowed', "Method $method not supported");
    }
}

# ============================================================================
# LEDGER API - Blockchain/state ledger
# ============================================================================

sub handle_ledger_api {
    my ($q, $method) = @_;
    
    if ($method eq 'GET') {
        my $action = $q->param('action') || 'latest';
        
        if ($action eq 'latest') {
            # Get latest ledger state
            my $ledger_file = "$DATA_DIR/ledger_state.json";
            my $ledger;
            
            if (-f $ledger_file) {
                open my $fh, '<', $ledger_file;
                local $/;
                $ledger = decode_json(<$fh>);
                close $fh;
            } else {
                $ledger = {
                    height => 0,
                    hash => '0' x 64,
                    timestamp => time(),
                    entries => []
                };
            }
            
            print encode_json({
                status => 'success',
                ledger => $ledger
            });
            
        } elsif ($action eq 'history') {
            # Get ledger history
            my $limit = $q->param('limit') || 100;
            
            print encode_json({
                status => 'success',
                message => 'Ledger history endpoint',
                limit => $limit
            });
        }
        
    } elsif ($method eq 'POST') {
        # Add entry to ledger
        my $entry_type = $q->param('type');
        my $data = $q->param('data');
        
        my $entry = {
            type => $entry_type,
            data => $data,
            timestamp => time(),
            hash => sha256_hex("$entry_type:$data:" . time())
        };
        
        # Update ledger
        my $ledger_file = "$DATA_DIR/ledger_state.json";
        my $ledger;
        
        if (-f $ledger_file) {
            open my $fh, '<', $ledger_file;
            local $/;
            $ledger = decode_json(<$fh>);
            close $fh;
        } else {
            $ledger = { height => 0, entries => [] };
        }
        
        push @{$ledger->{entries}}, $entry;
        $ledger->{height}++;
        $ledger->{hash} = sha256_hex(encode_json($ledger->{entries}));
        $ledger->{timestamp} = time();
        
        open my $fh, '>', $ledger_file;
        print $fh encode_json($ledger);
        close $fh;
        
        print encode_json({
            status => 'success',
            entry => $entry,
            ledger_height => $ledger->{height}
        });
    } else {
        error_response(405, 'Method Not Allowed', "Method $method not supported");
    }
}

# ============================================================================
# NETWORK API - Peer network status
# ============================================================================

sub handle_network_api {
    my ($q, $method) = @_;
    
    if ($method eq 'GET') {
        my $action = $q->param('action') || 'status';
        
        if ($action eq 'status') {
            print encode_json({
                status => 'success',
                network => {
                    connected_peers => 12,
                    total_nodes => 156,
                    network_hash_rate => '1.2 TH/s',
                    avg_latency => 45.2,
                    uptime => 86400,
                    version => '1.0.0'
                }
            });
        } elsif ($action eq 'peers') {
            my @peers = (
                { id => generate_random_hex(20), ip => '192.168.1.100', reputation => 0.85 },
                { id => generate_random_hex(20), ip => '10.0.0.50', reputation => 0.92 },
                { id => generate_random_hex(20), ip => '172.16.0.25', reputation => 0.78 }
            );
            
            print encode_json({
                status => 'success',
                peers => \@peers,
                count => scalar @peers
            });
        }
    } else {
        error_response(405, 'Method Not Allowed', "Method $method not supported");
    }
}

# ============================================================================
# SEARCH API - Content discovery
# ============================================================================

sub handle_search_api {
    my ($q, $method) = @_;
    
    if ($method eq 'GET') {
        my $query = $q->param('q') || '';
        my $type = $q->param('type') || 'all';
        
        my @results;
        
        # Search Things
        if ($type eq 'all' || $type eq 'thing') {
            opendir my $dh, $DATA_DIR;
            while (my $file = readdir $dh) {
                next unless $file =~ /^thing_(.+)\.json$/;
                my $thing_id = $1;
                
                # Simple search (would be more sophisticated in real implementation)
                if (!$query || $thing_id =~ /$query/i) {
                    push @results, {
                        type => 'thing',
                        id => $thing_id,
                        url => "/api/thing?id=$thing_id"
                    };
                }
            }
            closedir $dh;
        }
        
        # Search Identities
        if ($type eq 'all' || $type eq 'identity') {
            opendir my $dh, $DATA_DIR;
            while (my $file = readdir $dh) {
                next unless $file =~ /^identity_(.+)\.json$/;
                my $node_id = $1;
                
                if (!$query || $node_id =~ /$query/i) {
                    push @results, {
                        type => 'identity',
                        id => $node_id,
                        url => "/api/identity?id=$node_id"
                    };
                }
            }
            closedir $dh;
        }
        
        print encode_json({
            status => 'success',
            query => $query,
            results => \@results,
            count => scalar @results
        });
    } else {
        error_response(405, 'Method Not Allowed', "Method $method not supported");
    }
}

# ============================================================================
# STATS API - Network statistics and analytics
# ============================================================================

sub handle_stats_api {
    my ($q, $method) = @_;
    
    if ($method eq 'GET') {
        # Count various items
        my $thing_count = count_files($DATA_DIR, 'thing_');
        my $identity_count = count_files($DATA_DIR, 'identity_');
        my $key_count = count_files($DATA_DIR, 'key_');
        my $attestation_count = count_files($DATA_DIR, 'attestation_');
        
        print encode_json({
            status => 'success',
            stats => {
                things => $thing_count,
                identities => $identity_count,
                keys => $key_count,
                attestations => $attestation_count,
                network => {
                    uptime => 86400,
                    peers => 12,
                    total_nodes => 156
                },
                performance => {
                    avg_response_time => 23.5,
                    requests_per_second => 150,
                    cache_hit_rate => 0.87
                }
            }
        });
    } else {
        error_response(405, 'Method Not Allowed', "Method $method not supported");
    }
}

# ============================================================================
# VOUCH API - Trust vouching system
# ============================================================================

sub handle_vouch_api {
    my ($q, $method) = @_;
    
    if ($method eq 'POST') {
        my $voucher = $q->param('voucher');
        my $vouchee = $q->param('vouchee');
        my $strength = $q->param('strength') || 0.5;
        
        my $vouch = {
            id => generate_random_hex(32),
            voucher => $voucher,
            vouchee => $vouchee,
            strength => $strength,
            timestamp => time()
        };
        
        my $vouch_file = "$DATA_DIR/vouch_$vouch->{id}.json";
        open my $fh, '>', $vouch_file;
        print $fh encode_json($vouch);
        close $fh;
        
        print encode_json({
            status => 'success',
            vouch => $vouch
        });
        
    } elsif ($method eq 'GET') {
        my $node_id = $q->param('node_id');
        
        my @vouches;
        opendir my $dh, $DATA_DIR;
        while (my $file = readdir $dh) {
            next unless $file =~ /^vouch_(.+)\.json$/;
            open my $fh, '<', "$DATA_DIR/$file";
            local $/;
            my $vouch = decode_json(<$fh>);
            close $fh;
            
            if ($node_id && ($vouch->{voucher} eq $node_id || $vouch->{vouchee} eq $node_id)) {
                push @vouches, $vouch;
            } elsif (!$node_id) {
                push @vouches, $vouch;
            }
        }
        closedir $dh;
        
        print encode_json({
            status => 'success',
            vouches => \@vouches,
            count => scalar @vouches
        });
    } else {
        error_response(405, 'Method Not Allowed', "Method $method not supported");
    }
}

# ============================================================================
# Helper Functions
# ============================================================================

sub error_response {
    my ($code, $status, $message) = @_;
    
    print encode_json({
        error => {
            code => $code,
            status => $status,
            message => $message
        }
    });
}

sub generate_random_hex {
    my ($length) = @_;
    my @chars = ('0'..'9', 'a'..'f');
    return join '', map { $chars[rand @chars] } 1..$length;
}

sub calculate_difficulty {
    # Simplified difficulty calculation
    return 1000 + int(rand(500));
}

sub verify_pow_solution {
    my ($solution, $nonce) = @_;
    # Simplified verification
    return defined($solution) && defined($nonce);
}

sub get_node_reputation {
    my ($node_id) = @_;
    
    # Calculate from attestations
    my $total_rating = 0;
    my $count = 0;
    
    opendir my $dh, $DATA_DIR;
    while (my $file = readdir $dh) {
        next unless $file =~ /^attestation_(.+)\.json$/;
        open my $fh, '<', "$DATA_DIR/$file";
        local $/;
        my $att = decode_json(<$fh>);
        close $fh;
        
        if ($att->{attestee} eq $node_id) {
            $total_rating += $att->{rating};
            $count++;
        }
    }
    closedir $dh;
    
    return {
        score => $count > 0 ? $total_rating / $count / 10.0 : 0.0,
        attestations => $count,
        rank => int(rand(1000)) + 1
    };
}

sub get_top_reputation_nodes {
    my ($limit) = @_;
    
    my @nodes;
    opendir my $dh, $DATA_DIR;
    while (my $file = readdir $dh) {
        next unless $file =~ /^identity_(.+)\.json$/;
        my $node_id = $1;
        open my $fh, '<', "$DATA_DIR/$file";
        local $/;
        my $identity = decode_json(<$fh>);
        close $fh;
        
        my $rep = get_node_reputation($node_id);
        push @nodes, {
            node_id => $node_id,
            name => $identity->{name},
            reputation => $rep->{score}
        };
    }
    closedir $dh;
    
    @nodes = sort { $b->{reputation} <=> $a->{reputation} } @nodes;
    return @nodes[0..$limit-1];
}

sub update_reputation {
    my ($node_id, $rating) = @_;
    # Update reputation score (simplified)
    # In real implementation, this would update the ledger
}

sub count_files {
    my ($dir, $prefix) = @_;
    my $count = 0;
    opendir my $dh, $dir;
    while (my $file = readdir $dh) {
        $count++ if $file =~ /^$prefix/;
    }
    closedir $dh;
    return $count;
}
