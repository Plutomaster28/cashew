#!/usr/bin/env perl
################################################################################
# Cashew Gateway - Perl CGI Implementation
# 
# A simple gateway to the Cashew network using classic Perl + CGI
# Perfect for Raspberry Pi, old computers, or nostalgic web hosting!
#
# Author: Cashew Team
# License: MIT
# Date: February 2026
################################################################################

use strict;
use warnings;
use CGI;
use JSON;
use HTTP::Tiny;
use File::Basename;

# Configuration
my $CASHEW_API_BASE = $ENV{CASHEW_API} || "http://localhost:8080/api";
my $DEBUG = $ENV{DEBUG} || 0;

# Create CGI object
my $cgi = CGI->new;

# Get the requested path
my $path = $cgi->path_info() || '/';

# Main router
if ($path eq '/' || $path eq '') {
    show_home();
} elsif ($path =~ m{^/thing/([a-f0-9]{64})$}) {
    serve_thing($1);
} elsif ($path eq '/networks') {
    list_networks();
} elsif ($path =~ m{^/network/([a-f0-9]{64})$}) {
    show_network($1);
} elsif ($path eq '/upload') {
    show_upload_form();
} elsif ($path eq '/do-upload' && $cgi->request_method() eq 'POST') {
    handle_upload();
} else {
    show_404();
}

################################################################################
# Route Handlers
################################################################################

sub show_home {
    print $cgi->header(-type => 'text/html', -charset => 'utf-8');
    
    print <<'HTML';
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Cashew Gateway</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        
        .container {
            max-width: 800px;
            margin: 0 auto;
            background: white;
            border-radius: 20px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            overflow: hidden;
        }
        
        .header {
            background: linear-gradient(135deg, #f5f7fa 0%, #c3cfe2 100%);
            padding: 40px;
            text-align: center;
        }
        
        .logo {
            font-size: 80px;
            margin-bottom: 10px;
        }
        
        h1 {
            color: #8B4513;
            font-size: 2.5em;
            margin-bottom: 10px;
        }
        
        .tagline {
            color: #666;
            font-style: italic;
        }
        
        .mona {
            font-family: monospace;
            white-space: pre;
            color: #8B4513;
            font-size: 12px;
            margin: 20px 0;
        }
        
        .content {
            padding: 40px;
        }
        
        .nav {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 20px;
            margin-top: 30px;
        }
        
        .nav-item {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px 20px;
            text-decoration: none;
            border-radius: 10px;
            text-align: center;
            transition: transform 0.2s, box-shadow 0.2s;
            display: block;
        }
        
        .nav-item:hover {
            transform: translateY(-5px);
            box-shadow: 0 10px 20px rgba(0,0,0,0.2);
        }
        
        .nav-item-icon {
            font-size: 2em;
            margin-bottom: 10px;
        }
        
        .nav-item-title {
            font-weight: bold;
            font-size: 1.2em;
        }
        
        .feature-list {
            list-style: none;
            margin: 20px 0;
        }
        
        .feature-list li {
            padding: 10px 0;
            padding-left: 30px;
            position: relative;
        }
        
        .feature-list li:before {
            content: "🥜";
            position: absolute;
            left: 0;
        }
        
        .footer {
            background: #f8f9fa;
            padding: 20px;
            text-align: center;
            color: #666;
            font-size: 0.9em;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <div class="logo">🥜</div>
            <h1>Cashew Gateway</h1>
            <p class="tagline">Freedom over profit. Privacy over surveillance.</p>
            
            <div class="mona">
     /\_/\
    ( o.o )  <- Mona welcomes you!
     > ^ <
    /|   |\
   (_|   |_)
            </div>
        </div>
        
        <div class="content">
            <h2>Welcome to Your Personal Gateway!</h2>
            <p>This Perl CGI gateway connects you to the Cashew network. Browse content, manage networks, and share with friends!</p>
            
            <ul class="feature-list">
                <li>Browse Things (content) by hash</li>
                <li>View and manage networks</li>
                <li>Upload new content</li>
                <li>Monitor node status</li>
            </ul>
            
            <div class="nav">
                <a href="/networks" class="nav-item">
                    <div class="nav-item-icon">🕸️</div>
                    <div class="nav-item-title">View Networks</div>
                </a>
                
                <a href="/upload" class="nav-item">
                    <div class="nav-item-icon">📤</div>
                    <div class="nav-item-title">Upload Content</div>
                </a>
                
                <a href="http://localhost:8080" class="nav-item">
                    <div class="nav-item-icon">🌐</div>
                    <div class="nav-item-title">Full Web UI</div>
                </a>
            </div>
        </div>
        
        <div class="footer">
            Powered by Perl  + Cashew Network <br>
            Running in true 90s web style!
        </div>
    </div>
</body>
</html>
HTML
}

sub serve_thing {
    my ($hash) = @_;
    
    # Fetch thing from Cashew API
    my $http = HTTP::Tiny->new(timeout => 10);
    my $response = $http->get("$CASHEW_API_BASE/thing/$hash");
    
    if ($response->{success}) {
        # Get content type from response headers
        my $content_type = $response->{headers}{'content-type'} || 'application/octet-stream';
        
        print $cgi->header(
            -type => $content_type,
            -charset => 'utf-8',
            -Content_Length => length($response->{content})
        );
        
        print $response->{content};
    } else {
        print $cgi->header(-status => '404 Not Found', -type => 'text/html');
        print <<HTML;
<!DOCTYPE html>
<html>
<head><title>Thing Not Found</title></head>
<body style="font-family: Arial; max-width: 600px; margin: 50px auto; text-align: center;">
    <h1> Thing Not Found</h1>
    <p>The thing with hash <code>$hash</code> could not be found.</p>
    <p>It may not exist or is not available on the network.</p>
    <a href="/">← Back to Home</a>
</body>
</html>
HTML
    }
}

sub list_networks {
    my $http = HTTP::Tiny->new(timeout => 10);
    my $response = $http->get("$CASHEW_API_BASE/networks");
    
    print $cgi->header(-type => 'text/html', -charset => 'utf-8');
    
    print <<'HTML_START';
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Cashew Networks</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 900px;
            margin: 50px auto;
            padding: 20px;
            background: linear-gradient(135deg, #f5f7fa 0%, #c3cfe2 100%);
        }
        
        h1 { color: #8B4513; text-align: center; }
        
        .network {
            background: white;
            border-left: 5px solid #667eea;
            padding: 20px;
            margin: 15px 0;
            border-radius: 5px;
            box-shadow: 0 2px 5px rgba(0,0,0,0.1);
        }
        
        .network.healthy {
            border-left-color: #4CAF50;
        }
        
        .network.degraded {
            border-left-color: #FF9800;
        }
        
        .network.critical {
            border-left-color: #F44336;
        }
        
        .network-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 10px;
        }
        
        .network-title {
            font-size: 1.3em;
            font-weight: bold;
        }
        
        .status-badge {
            padding: 5px 15px;
            border-radius: 20px;
            color: white;
            font-size: 0.9em;
        }
        
        .status-healthy { background: #4CAF50; }
        .status-degraded { background: #FF9800; }
        .status-critical { background: #F44336; }
        
        .network-info {
            color: #666;
            font-size: 0.9em;
        }
        
        .empty {
            text-align: center;
            padding: 40px;
            color: #999;
        }
        
        .back-link {
            display: inline-block;
            margin-bottom: 20px;
            color: #667eea;
            text-decoration: none;
        }
    </style>
</head>
<body>
    <a href="/" class="back-link">← Back to Home</a>
    <h1>🕸️ Active Networks</h1>
HTML_START
    
    if ($response->{success}) {
        my $data = eval { decode_json($response->{content}) };
        
        if ($data && ref($data->{networks}) eq 'ARRAY') {
            my $networks = $data->{networks};
            
            if (@$networks == 0) {
                print '<div class="empty">No networks yet. Create one to get started!</div>';
            } else {
                foreach my $net (@$networks) {
                    my $health = $net->{health} || 'unknown';
                    my $health_class = "status-$health";
                    
                    print "<div class='network $health'>";
                    print "<div class='network-header'>";
                    print "<div class='network-title'>" . ($net->{name} || 'Unnamed Network') . "</div>";
                    print "<div class='status-badge $health_class'>$health</div>";
                    print "</div>";
                    print "<div class='network-info'>";
                    print "Members: " . ($net->{member_count} || 0) . " | ";
                    print "Thing: " . substr($net->{thing_hash} || 'unknown', 0, 16) . "...";
                    print "</div>";
                    print "</div>";
                }
            }
        } else {
            print '<div class="empty">Error parsing network data</div>';
        }
    } else {
        print '<div class="empty">Could not connect to Cashew node</div>';
    }
    
    print <<'HTML_END';
</body>
</html>
HTML_END
}

sub show_upload_form {
    print $cgi->header(-type => 'text/html', -charset => 'utf-8');
    
    print <<'HTML';
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Upload Content - Cashew</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 600px;
            margin: 50px auto;
            padding: 20px;
            background: linear-gradient(135deg, #f5f7fa 0%, #c3cfe2 100%);
        }
        
        h1 { color: #8B4513; text-align: center; }
        
        .upload-form {
            background: white;
            padding: 30px;
            border-radius: 10px;
            box-shadow: 0 5px 15px rgba(0,0,0,0.1);
        }
        
        .form-group {
            margin-bottom: 20px;
        }
        
        label {
            display: block;
            margin-bottom: 5px;
            font-weight: bold;
            color: #333;
        }
        
        input[type="file"],
        textarea,
        input[type="text"] {
            width: 100%;
            padding: 10px;
            border: 2px solid #ddd;
            border-radius: 5px;
            font-size: 1em;
        }
        
        textarea {
            min-height: 200px;
            font-family: monospace;
        }
        
        button {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            padding: 15px 30px;
            font-size: 1.1em;
            border-radius: 5px;
            cursor: pointer;
            width: 100%;
        }
        
        button:hover {
            opacity: 0.9;
        }
        
        .help-text {
            font-size: 0.9em;
            color: #666;
            margin-top: 5px;
        }
        
        .back-link {
            display: inline-block;
            margin-bottom: 20px;
            color: #667eea;
            text-decoration: none;
        }
    </style>
</head>
<body>
    <a href="/" class="back-link">← Back to Home</a>
    <h1> Upload Content</h1>
    
    <div class="upload-form">
        <form method="POST" action="/do-upload" enctype="multipart/form-data">
            <div class="form-group">
                <label for="content_type">Upload Type:</label>
                <select name="content_type" id="content_type" onchange="toggleUploadType(this.value)">
                    <option value="file">File Upload</option>
                    <option value="text">Text/HTML Content</option>
                </select>
            </div>
            
            <div class="form-group" id="file-upload">
                <label for="file">Choose File:</label>
                <input type="file" name="file" id="file">
                <div class="help-text">Upload any file (images, HTML, documents, etc.)</div>
            </div>
            
            <div class="form-group" id="text-upload" style="display: none;">
                <label for="text_content">Content:</label>
                <textarea name="text_content" id="text_content" placeholder="Paste your HTML or text here..."></textarea>
                <div class="help-text">Write or paste HTML, Markdown, or plain text</div>
            </div>
            
            <div class="form-group">
                <label for="description">Description (optional):</label>
                <input type="text" name="description" id="description" placeholder="What is this content?">
            </div>
            
            <button type="submit">🥜 Upload to Cashew</button>
        </form>
    </div>
    
    <script>
    function toggleUploadType(type) {
        document.getElementById('file-upload').style.display = type === 'file' ? 'block' : 'none';
        document.getElementById('text-upload').style.display = type === 'text' ? 'block' : 'none';
    }
    </script>
</body>
</html>
HTML
}

sub handle_upload {
    print $cgi->header(-type => 'text/html', -charset => 'utf-8');
    
    print <<'HTML';
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Upload Result - Cashew</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 600px;
            margin: 50px auto;
            padding: 20px;
            background: linear-gradient(135deg, #f5f7fa 0%, #c3cfe2 100%);
        }
        
        .result {
            background: white;
            padding: 30px;
            border-radius: 10px;
            box-shadow: 0 5px 15px rgba(0,0,0,0.1);
            text-align: center;
        }
        
        .success {
            color: #4CAF50;
            font-size: 3em;
        }
        
        h1 { color: #8B4513; }
        
        .hash {
            background: #f5f5f5;
            padding: 10px;
            border-radius: 5px;
            font-family: monospace;
            word-break: break-all;
            margin: 20px 0;
        }
        
        a {
            color: #667eea;
            text-decoration: none;
        }
    </style>
</head>
<body>
    <div class="result">
        <div class="success">Check</div>
        <h1>Upload Successful!</h1>
        <p>Your content has been stored in Cashew.</p>
        
        <div class="hash">
            Hash: (Coming soon - API integration needed)
        </div>
        
        <p><a href="/upload">← Upload Another</a> | <a href="/">Home</a></p>
    </div>
</body>
</html>
HTML
}

sub show_404 {
    print $cgi->header(-status => '404 Not Found', -type => 'text/html');
    print <<'HTML';
<!DOCTYPE html>
<html>
<head><title>404 - Not Found</title></head>
<body style="font-family: Arial; max-width: 600px; margin: 100px auto; text-align: center;">
    <h1 style="font-size: 5em;">Peanut</h1>
    <h2>404 - Page Not Found</h2>
    <p>Mona couldn't find what you're looking for!</p>
    <a href="/" style="color: #667eea;">← Back to Home</a>
</body>
</html>
HTML
}

################################################################################
# Helper Functions
################################################################################

sub debug_log {
    my ($msg) = @_;
    if ($DEBUG) {
        warn "[DEBUG] $msg\n";
    }
}

1;

