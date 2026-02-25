#!/usr/bin/env perl
# Simple HTTP server for Cashew Perl CGI demo
# Runs the full-featured demo site

use strict;
use warnings;
use HTTP::Server::Simple::CGI;
use File::Basename;
use File::Spec;
use Cwd 'abs_path';

package CashewDemoServer;
use base qw(HTTP::Server::Simple::CGI);

my $base_dir = dirname(abs_path(__FILE__));

sub handle_request {
    my ($self, $cgi) = @_;
    
    my $path = $cgi->path_info();
    $path = '/index.html' if $path eq '/' || $path eq '';
    
    # Handle API requests
    if ($path =~ m{^/api/}) {
        # Set environment for CGI script
        $ENV{PATH_INFO} = $path;
        $ENV{REQUEST_METHOD} = $cgi->request_method();
        $ENV{CASHEW_DATA_DIR} = "$base_dir/data";
        
        # Execute CGI script
        my $script = "$base_dir/cashew-api.pl";
        if (-f $script) {
            # Make executable if not already
            chmod 0755, $script;
            
            # Execute the CGI script
            do $script;
            if ($@) {
                print "HTTP/1.0 500 Internal Server Error\r\n";
                print "Content-Type: text/plain\r\n\r\n";
                print "CGI Error: $@\n";
            }
        } else {
            print "HTTP/1.0 404 Not Found\r\n";
            print "Content-Type: text/plain\r\n\r\n";
            print "CGI script not found\n";
        }
        return;
    }
    
    # Serve static files
    my $file = "$base_dir$path";
    
    if (-f $file) {
        open my $fh, '<', $file or do {
            print "HTTP/1.0 404 Not Found\r\n";
            print "Content-Type: text/plain\r\n\r\n";
            print "File not found\n";
            return;
        };
        
        # Determine content type
        my $content_type = 'text/plain';
        if ($path =~ /\.html?$/) {
            $content_type = 'text/html';
        } elsif ($path =~ /\.css$/) {
            $content_type = 'text/css';
        } elsif ($path =~ /\.js$/) {
            $content_type = 'application/javascript';
        } elsif ($path =~ /\.json$/) {
            $content_type = 'application/json';
        } elsif ($path =~ /\.(jpg|jpeg)$/) {
            $content_type = 'image/jpeg';
        } elsif ($path =~ /\.png$/) {
            $content_type = 'image/png';
        }
        
        print "HTTP/1.0 200 OK\r\n";
        print "Content-Type: $content_type\r\n";
        print "Access-Control-Allow-Origin: *\r\n";
        print "\r\n";
        
        binmode $fh;
        print while <$fh>;
        close $fh;
    } else {
        print "HTTP/1.0 404 Not Found\r\n";
        print "Content-Type: text/plain\r\n\r\n";
        print "404 Not Found: $path\n";
    }
}

package main;

# Create data directory
my $data_dir = "$base_dir/data";
mkdir $data_dir unless -d $data_dir;

print "=" x 70 . "\n";
print "🥜 Cashew Network - Full Demo Server\n";
print "=" x 70 . "\n\n";
print "Server starting on http://localhost:8000\n\n";
print "Features available:\n";
print "  ✓ Content Publishing (Things) - up to 500MB\n";
print "  ✓ Identity Management - Ed25519 keys\n";
print "  ✓ Participation Keys - 5 types with decay\n";
print "  ✓ Proof-of-Work - Argon2id puzzles\n";
print "  ✓ Reputation System - Trust attestations\n";
print "  ✓ Ledger - Blockchain state\n";
print "  ✓ Network Status - P2P monitoring\n";
print "  ✓ Statistics - Real-time metrics\n\n";
print "Data directory: $data_dir\n";
print "\nPress Ctrl+C to stop the server\n";
print "=" x 70 . "\n\n";

my $server = CashewDemoServer->new(8000);
$server->run();
