#!/usr/bin/env perl
use strict;
use warnings;
use HTTP::Server::Simple::CGI;
use File::Basename qw(dirname);
use Cwd qw(abs_path);

package MiyamiiCGIServer;
use base qw(HTTP::Server::Simple::CGI);

my $base_dir = abs_path(dirname(__FILE__) . '/../../test-site');
my $guestbook_cgi = "$base_dir/miyamii/cgi-bin/guestbook.pl";

sub _content_type {
    my ($path) = @_;
    return 'text/html; charset=UTF-8' if $path =~ /\.html?$/i;
    return 'text/css; charset=UTF-8' if $path =~ /\.css$/i;
    return 'application/javascript; charset=UTF-8' if $path =~ /\.js$/i;
    return 'application/json; charset=UTF-8' if $path =~ /\.json$/i;
    return 'image/png' if $path =~ /\.png$/i;
    return 'image/jpeg' if $path =~ /\.jpe?g$/i;
    return 'image/gif' if $path =~ /\.gif$/i;
    return 'text/plain; charset=UTF-8';
}

sub handle_request {
    my ($self, $cgi) = @_;

    my $path = $cgi->path_info() || '/';
    $path = '/index.html' if $path eq '/';

    if ($path eq '/miyamii/cgi-bin/guestbook.pl') {
        if (!-f $guestbook_cgi) {
            print "HTTP/1.0 500 Internal Server Error\r\n";
            print "Content-Type: text/plain\r\n\r\n";
            print "guestbook CGI not found\n";
            return;
        }

        local $ENV{REQUEST_METHOD} = $cgi->request_method();
        local $ENV{CONTENT_TYPE} = $cgi->content_type() || '';
        local $ENV{CONTENT_LENGTH} = $cgi->content_length() || 0;
        local $ENV{QUERY_STRING} = $cgi->query_string() || '';
        local $ENV{PATH_INFO} = $path;

        {
            no warnings 'redefine';
            do $guestbook_cgi;
        }
        if ($@) {
            print "HTTP/1.0 500 Internal Server Error\r\n";
            print "Content-Type: text/plain\r\n\r\n";
            print "CGI execution failed: $@\n";
        }
        return;
    }

    my $file = "$base_dir$path";
    if (-d $file) {
        $file .= '/index.html';
    }

    if (!-f $file) {
        print "HTTP/1.0 404 Not Found\r\n";
        print "Content-Type: text/plain\r\n\r\n";
        print "Not found: $path\n";
        return;
    }

    open my $fh, '<', $file or do {
        print "HTTP/1.0 500 Internal Server Error\r\n";
        print "Content-Type: text/plain\r\n\r\n";
        print "Failed to open: $path\n";
        return;
    };

    binmode $fh;

    print "HTTP/1.0 200 OK\r\n";
    print "Content-Type: " . _content_type($file) . "\r\n";
    print "Cache-Control: no-store\r\n\r\n";
    print while <$fh>;
    close $fh;
}

package main;

my $port = 8090;
if (@ARGV && $ARGV[0] =~ /^\d+$/) {
    $port = int($ARGV[0]);
}

print "Miyamii demo CGI server\n";
print "Serving: $base_dir\n";
print "URL: http://localhost:$port/miyamii/\n";
print "Press Ctrl+C to stop\n\n";

my $server = MiyamiiCGIServer->new($port);
$server->run();
