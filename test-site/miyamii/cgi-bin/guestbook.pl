#!/usr/bin/env perl
use strict;
use warnings;
use JSON::PP qw(encode_json decode_json);
use File::Basename qw(dirname);
use File::Path qw(make_path);
use POSIX qw(strftime);

my $script_dir = dirname(__FILE__);
my $data_dir = "$script_dir/../data";
my $data_file = "$data_dir/guestbook.json";

sub _read_request_body {
    my $len = $ENV{CONTENT_LENGTH} || 0;
    return '' if !$len;
    my $body = '';
    read(STDIN, $body, $len);
    return $body;
}

sub _ensure_data_file {
    if (!-d $data_dir) {
        make_path($data_dir);
    }

    if (!-f $data_file) {
        my $initial = {
            visits => 0,
            last_num => 3,
            posts => []
        };
        open my $fh, '>', $data_file or return;
        print {$fh} encode_json($initial);
        close $fh;
    }
}

sub _load_state {
    _ensure_data_file();

    open my $fh, '<', $data_file or return { visits => 0, last_num => 3, posts => [] };
    local $/;
    my $raw = <$fh>;
    close $fh;

    my $decoded;
    eval { $decoded = decode_json($raw); };
    if ($@ || ref($decoded) ne 'HASH') {
        return { visits => 0, last_num => 3, posts => [] };
    }

    $decoded->{visits} = 0 if !defined $decoded->{visits};
    $decoded->{last_num} = 3 if !defined $decoded->{last_num};
    $decoded->{posts} = [] if ref($decoded->{posts}) ne 'ARRAY';

    return $decoded;
}

sub _save_state {
    my ($state) = @_;
    open my $fh, '>', $data_file or return 0;
    print {$fh} encode_json($state);
    close $fh;
    return 1;
}

sub _json_response {
    my ($status, $payload) = @_;
    print "Status: $status\r\n";
    print "Content-Type: application/json; charset=UTF-8\r\n";
    print "Access-Control-Allow-Origin: *\r\n";
    print "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    print "Access-Control-Allow-Headers: Content-Type\r\n\r\n";
    print encode_json($payload);
}

sub _make_post {
    my ($state, $name, $subject, $body) = @_;

    $state->{last_num} += 1;
    my $now = strftime('%Y/%m/%d(%a) %H:%M:%S', localtime());
    my $id = uc(substr(unpack('H*', pack('N', int(rand(0xFFFFFFFF)))), 0, 6));

    return {
        num => $state->{last_num},
        name => $name,
        subject => $subject,
        body => $body,
        date => $now,
        id => $id
    };
}

my $method = $ENV{REQUEST_METHOD} || 'GET';

if ($method eq 'OPTIONS') {
    _json_response('200 OK', { ok => JSON::PP::true });
    exit;
}

my $state = _load_state();
$state->{visits} += 1;

if ($method eq 'GET') {
    _save_state($state);
    _json_response('200 OK', {
        visits => $state->{visits},
        posts => $state->{posts}
    });
    exit;
}

if ($method eq 'POST') {
    my $raw = _read_request_body();
    my $payload;
    eval { $payload = decode_json($raw); };
    if ($@ || ref($payload) ne 'HASH') {
        _json_response('400 Bad Request', { error => 'Invalid JSON body' });
        exit;
    }

    my $name = $payload->{name};
    $name = 'Anonymous' if !defined($name) || $name eq '';
    my $subject = $payload->{subject};
    $subject = '' if !defined $subject;
    my $body = $payload->{body};

    if (!defined($body) || $body !~ /\S/) {
        _json_response('400 Bad Request', { error => 'Post body is required' });
        exit;
    }

    $name = substr($name, 0, 30);
    $subject = substr($subject, 0, 80);
    $body = substr($body, 0, 4000);

    my $post = _make_post($state, $name, $subject, $body);
    push @{$state->{posts}}, $post;

    if (@{$state->{posts}} > 300) {
        my @trimmed = @{$state->{posts}}[-300 .. -1];
        $state->{posts} = \@trimmed;
    }

    if (!_save_state($state)) {
        _json_response('500 Internal Server Error', { error => 'Failed to persist post' });
        exit;
    }

    _json_response('200 OK', {
        ok => JSON::PP::true,
        visits => $state->{visits},
        post => $post
    });
    exit;
}

_json_response('405 Method Not Allowed', { error => 'Unsupported method' });
