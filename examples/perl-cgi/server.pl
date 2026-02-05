#!/usr/bin/env perl
use strict;
use warnings;
use HTTP::Daemon;
use HTTP::Status;
use JSON;
use HTTP::Tiny;

# Configuration
my $CASHEW_API_BASE = $ENV{CASHEW_API} || "http://127.0.0.1:8080/api";
my $PORT = $ENV{PORT} || 8081;

print "Starting Cashew Gateway on http://localhost:$PORT/\n";
print "Connecting to Cashew API at $CASHEW_API_BASE\n";

my $d = HTTP::Daemon->new(
    LocalPort => $PORT,
    ReuseAddr => 1,
) || die "Cannot start server: $!";

print "Server running at: ", $d->url, "\n";
print "Press Ctrl+C to stop\n\n";

while (my $c = $d->accept) {
    while (my $r = $c->get_request) {
        my $path = $r->uri->path;
        
        print "[" . localtime() . "] " . $r->method . " $path\n";
        
        if ($path eq '/' || $path eq '') {
            serve_home($c, $r);
        } elsif ($path =~ m{^/thing/([a-f0-9]{64})$}) {
            serve_thing($c, $r, $1);
        } elsif ($path eq '/networks') {
            list_networks($c, $r);
        } elsif ($path =~ m{^/network/([a-f0-9]{64})$}) {
            show_network($c, $r, $1);
        } else {
            serve_404($c, $r);
        }
    }
    $c->close;
    undef($c);
}

sub serve_home {
    my ($c, $r) = @_;
    
    my $html = <<'HTML';
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>（　´∀｀）Cashew Gateway</title>
    <style>
        body {
            font-family: "MS PGothic", "Mona", monospace;
            background: #ffffee;
            color: #000000;
            margin: 0;
            padding: 20px;
            line-height: 1.6;
        }
        .container {
            max-width: 900px;
            margin: 0 auto;
            background: #ffffff;
            border: 3px solid #800000;
            padding: 15px;
        }
        h1 {
            background: #800000;
            color: #ffffff;
            padding: 10px;
            margin: -15px -15px 15px -15px;
            font-size: 16pt;
            border-bottom: 2px solid #000;
        }
        .mona {
            font-family: "Mona", "MS PGothic", monospace;
            white-space: pre;
            background: #f0f0f0;
            padding: 10px;
            border: 1px solid #ccc;
            margin: 10px 0;
        }
        .menu {
            background: #f5f5dc;
            border: 2px solid #800000;
            padding: 10px;
            margin: 15px 0;
        }
        .menu a {
            color: #0000ee;
            text-decoration: none;
            margin-right: 20px;
        }
        .menu a:visited {
            color: #551a8b;
        }
        .menu a:hover {
            text-decoration: underline;
        }
        .status {
            background: #e0ffe0;
            border: 1px solid #00aa00;
            padding: 10px;
            margin: 10px 0;
        }
        .footer {
            margin-top: 20px;
            padding-top: 10px;
            border-top: 1px solid #ccc;
            font-size: 9pt;
            color: #666;
            text-align: center;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>（　´∀｀）Cashew P2P Network Gateway</h1>
        
        <div class="mona">
　　　　　　　　　　　　 ∧＿∧
　　　　　　　　　　　（　´∀｀）　　＜ Welcome to Cashew!
　　　　　　　　　　　（　　　　 ）
　　　　　　　　　　　 ｜　｜　|
　　　　　　　　　　　（＿_）＿）
        </div>
        
        <div class="status">
            <strong>Status:</strong> Online ● Connected to Cashew Network
        </div>
        
        <div class="menu">
            <strong>Menu:</strong><br>
            <a href="/">Home</a>
            <a href="/networks">View Networks</a>
        </div>
        
        <h2>About</h2>
        <p>
            This is a gateway to the Cashew decentralized network, running on your local machine.
            The Cashew network uses P2P technology to share content without central servers.
        </p>
        
        <h2>API Endpoints</h2>
        <ul>
            <li><a href="/networks">/networks</a> - List available networks</li>
            <li>/thing/[hash] - Retrieve content by hash</li>
            <li>/network/[hash] - View network details</li>
        </ul>
        
        <div class="footer">
            Powered by Cashew Network | Perl + HTTP::Daemon | Freedom over profit
        </div>
    </div>
</body>
</html>
HTML
    
    my $res = HTTP::Response->new(200);
    $res->header('Content-Type' => 'text/html; charset=UTF-8');
    $res->content($html);
    $c->send_response($res);
}

sub serve_thing {
    my ($c, $r, $hash) = @_;
    
    my $http = HTTP::Tiny->new(timeout => 2, keep_alive => 1);
    my $response = $http->get("$CASHEW_API_BASE/thing/$hash");
    
    if (!$response->{success}) {
        my $html = <<HTML;
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Not Found - Cashew</title>
</head>
<body style="font-family: monospace; background: #ffffee; padding: 20px;">
    <h1>404 - Thing Not Found</h1>
    <p>Content with hash <code>$hash</code> was not found on the network.</p>
    <p><a href="/">← Back to Home</a></p>
</body>
</html>
HTML
        
        my $res = HTTP::Response->new(404);
        $res->header('Content-Type' => 'text/html; charset=UTF-8');
        $res->content($html);
        $c->send_response($res);
        return;
    }
    
    # Forward the content from Cashew API
    my $res = HTTP::Response->new(200);
    $res->header('Content-Type' => $response->{headers}{'content-type'} || 'application/octet-stream');
    $res->content($response->{content});
    $c->send_response($res);
}

sub list_networks {
    my ($c, $r) = @_;
    
    my $http = HTTP::Tiny->new(timeout => 2, keep_alive => 1);
    my $response = $http->get("$CASHEW_API_BASE/networks");
    
    my $networks = [];
    if ($response->{success}) {
        eval {
            my $data = decode_json($response->{content});
            $networks = $data->{networks} || [];
        };
    }
    
    my $html = <<'HTML';
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Networks - Cashew</title>
    <style>
        body {
            font-family: "MS PGothic", "Mona", monospace;
            background: #ffffee;
            color: #000000;
            margin: 0;
            padding: 20px;
        }
        .container {
            max-width: 900px;
            margin: 0 auto;
            background: #ffffff;
            border: 3px solid #800000;
            padding: 15px;
        }
        h1 {
            background: #800000;
            color: #ffffff;
            padding: 10px;
            margin: -15px -15px 15px -15px;
        }
        .network {
            background: #f5f5dc;
            border: 1px solid #ccc;
            padding: 10px;
            margin: 10px 0;
        }
        a { color: #0000ee; }
        a:visited { color: #551a8b; }
    </style>
</head>
<body>
    <div class="container">
        <h1>（　´∀｀）Cashew Networks</h1>
        <p><a href="/">← Back to Home</a></p>
HTML
    
    if (@$networks == 0) {
        $html .= <<'HTML';
        <div class="network">
            <p><strong>No networks found</strong></p>
            <p>There are currently no networks available on this node.</p>
        </div>
HTML
    } else {
        foreach my $net (@$networks) {
            my $name = $net->{name} || 'Unknown';
            my $hash = $net->{hash} || '';
            my $members = $net->{members} || 0;
            
            $html .= <<HTML;
        <div class="network">
            <h3>$name</h3>
            <p>Hash: <code>$hash</code></p>
            <p>Members: $members</p>
            <p><a href="/network/$hash">View Details →</a></p>
        </div>
HTML
        }
    }
    
    $html .= <<'HTML';
        <div style="margin-top: 20px; padding-top: 10px; border-top: 1px solid #ccc; font-size: 9pt; color: #666; text-align: center;">
            Powered by Cashew Network
        </div>
    </div>
</body>
</html>
HTML
    
    my $res = HTTP::Response->new(200);
    $res->header('Content-Type' => 'text/html; charset=UTF-8');
    $res->content($html);
    $c->send_response($res);
}

sub show_network {
    my ($c, $r, $hash) = @_;
    
    my $html = <<HTML;
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Network Details - Cashew</title>
</head>
<body style="font-family: monospace; background: #ffffee; padding: 20px;">
    <h1>Network: $hash</h1>
    <p><a href="/networks">← Back to Networks</a></p>
    <p>Network details not yet implemented.</p>
</body>
</html>
HTML
    
    my $res = HTTP::Response->new(200);
    $res->header('Content-Type' => 'text/html; charset=UTF-8');
    $res->content($html);
    $c->send_response($res);
}

sub serve_404 {
    my ($c, $r) = @_;
    
    my $html = <<'HTML';
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>404 - Not Found</title>
</head>
<body style="font-family: monospace; background: #ffffee; padding: 20px;">
    <h1>404 - Page Not Found</h1>
    <p><a href="/">← Back to Home</a></p>
</body>
</html>
HTML
    
    my $res = HTTP::Response->new(404);
    $res->header('Content-Type' => 'text/html; charset=UTF-8');
    $res->content($html);
    $c->send_response($res);
}
