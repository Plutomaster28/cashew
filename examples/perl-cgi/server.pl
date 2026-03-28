#!/usr/bin/env perl
use strict;
use warnings;
use HTTP::Server::Simple::CGI;
use JSON::PP qw(encode_json decode_json);
use File::Basename qw(dirname);
use File::Path qw(make_path);
use Cwd qw(abs_path);
use POSIX qw(strftime);

package MiyamiiCGIServer;
use base qw(HTTP::Server::Simple::CGI);

my $base_dir = Cwd::abs_path(File::Basename::dirname(__FILE__) . '/../../test-site');
my $guestbook_data_dir = "$base_dir/miyamii/data";
my $guestbook_data_file = "$guestbook_data_dir/guestbook.json";

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

sub _ensure_guestbook_file {
    if (!-d $guestbook_data_dir) {
        File::Path::make_path($guestbook_data_dir);
    }

    if (!-f $guestbook_data_file) {
        my $seed = { visits => 0, last_num => 3, posts => [] };
        open my $fh, '>', $guestbook_data_file or return;
        print {$fh} JSON::PP::encode_json($seed);
        close $fh;
    }
}

sub _load_guestbook_state {
    _ensure_guestbook_file();

    open my $fh, '<', $guestbook_data_file or return { visits => 0, last_num => 3, posts => [] };
    local $/;
    my $raw = <$fh>;
    close $fh;

    my $state;
    eval { $state = JSON::PP::decode_json($raw); };
    if ($@ || ref($state) ne 'HASH') {
        return { visits => 0, last_num => 3, posts => [] };
    }

    $state->{visits} = 0 if !defined $state->{visits};
    $state->{last_num} = 3 if !defined $state->{last_num};
    $state->{posts} = [] if ref($state->{posts}) ne 'ARRAY';

    return $state;
}

sub _save_guestbook_state {
    my ($state) = @_;
    open my $fh, '>', $guestbook_data_file or return 0;
    print {$fh} JSON::PP::encode_json($state);
    close $fh;
    return 1;
}

sub _json_response {
    my ($status_line, $payload) = @_;
    print "HTTP/1.0 $status_line\r\n";
    print "Content-Type: application/json; charset=UTF-8\r\n";
    print "Access-Control-Allow-Origin: *\r\n";
    print "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    print "Access-Control-Allow-Headers: Content-Type\r\n\r\n";
    print JSON::PP::encode_json($payload);
}

sub _handle_guestbook {
    my ($cgi) = @_;
    my $method = $cgi->request_method() || 'GET';

    if ($method eq 'OPTIONS') {
        _json_response('200 OK', { ok => JSON::PP::true });
        return;
    }

    my $state = _load_guestbook_state();
    $state->{visits} += 1;

    if ($method eq 'GET') {
        _save_guestbook_state($state);
        _json_response('200 OK', {
            visits => $state->{visits},
            posts => $state->{posts}
        });
        return;
    }

    if ($method eq 'POST') {
        my $name = $cgi->param('name');
        my $subject = $cgi->param('subject');
        my $body = $cgi->param('body');

        # Fallback for JSON callers when CGI form params are absent.
        if (!defined($body) || $body !~ /\S/) {
            my $raw = $cgi->param('POSTDATA');
            $raw = '' if !defined $raw;
            if ($raw ne '') {
                my $payload;
                eval { $payload = JSON::PP::decode_json($raw); };
                if (!$@ && ref($payload) eq 'HASH') {
                    $name = $payload->{name};
                    $subject = $payload->{subject};
                    $body = $payload->{body};
                }
            }
        }

        $name = 'Anonymous' if !defined($name) || $name eq '';
        $subject = '' if !defined $subject;

        if (!defined($body) || $body !~ /\S/) {
            _json_response('400 Bad Request', { error => 'Post body is required' });
            return;
        }

        $name = substr($name, 0, 30);
        $subject = substr($subject, 0, 80);
        $body = substr($body, 0, 4000);

        $state->{last_num} += 1;
        my $now = POSIX::strftime('%Y/%m/%d(%a) %H:%M:%S', localtime());
        my $id = uc(substr(unpack('H*', pack('N', int(rand(0xFFFFFFFF)))), 0, 6));

        my $post = {
            num => $state->{last_num},
            name => $name,
            subject => $subject,
            body => $body,
            date => $now,
            id => $id
        };

        push @{$state->{posts}}, $post;
        if (@{$state->{posts}} > 300) {
            my @trimmed = @{$state->{posts}}[-300 .. -1];
            $state->{posts} = \@trimmed;
        }

        if (!_save_guestbook_state($state)) {
            _json_response('500 Internal Server Error', { error => 'Failed to persist post' });
            return;
        }

        _json_response('200 OK', {
            ok => JSON::PP::true,
            visits => $state->{visits},
            post => $post
        });
        return;
    }

    _json_response('405 Method Not Allowed', { error => 'Unsupported method' });
}

sub handle_request {
    my ($self, $cgi) = @_;

    my $path = $cgi->path_info() || '/';
    $path = '/index.html' if $path eq '/';

    if ($path eq '/miyamii/cgi-bin/guestbook.pl') {
        _handle_guestbook($cgi);
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
