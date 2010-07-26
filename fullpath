#! /usr/bin/perl
#
# $Id$
#
# Author: Boris Jakubith
# E-Mail: runkharr@googlemail.com
# Copyright: (c) 2010, Boris Jakubith <runkharr@googlemail.com>
# Released under GPL v2.
#

use strict 'vars';

sub ppsplit ($;$);

my ($pn, $pp) = ppsplit ($0);

unless (@ARGV) {
    print "Usage: $0 [-b <dir-level>] pathname...\n"; exit 64;
}

my $level = 0;
if ($_ = $ARGV[$[], /^-/) {
    shift; last if ($_ eq '--');
    if (/^-b(.*)/) {
	unless ($1 || @ARGV) {
	    print STDERR "$0: missing argument for option `-b´\n"; exit 64;
	}
	$level = ($1 ? $1 : shift);
	if ($level =~ /\D/) {
	    print STDERR "$0: `-b´ requires a numeric argument\n"; exit 64;
	}
    } else {
	print STDERR "$0: invalid option `$_´\n"; exit 64;
    }
}
unless (@ARGV) {
    print STDERR "$0: missing argument(s)\n"; exit 64;
}

for my $path (@ARGV) {
    my ($bn, $dn) = ppsplit ($path);
    if ($level > 0) {
	my @spp = ((split /\//, $dn), $bn);
	for (my $lvc = $level; $lvc > 0; --$lvc) {
	    last unless (@spp);
	    pop @spp;
	}
	while (@spp < 2) { push @spp, ''; }
	print +(join '/', @spp)."\n";
    } else {
	print "$dn/$bn\n";
    }
}

exit 0;

sub ppsplit ($;$) {
    use Cwd;
    my ($pp, $pdel) = @_;
    my (@pp, @res, $x, $pn);
    $pdel = '/' unless ($pdel);
    $x = qr($pdel); @pp = split /$x/, $pp;
    $pn = pop @pp;
    unless (@pp && (length $pp[$[]) == 0) {
	unshift @pp, (split $pdel, (cwd ()));
    }
    @res = ();
    while (@pp) {
	$x = shift @pp;
	if ($x eq '..') {
	    pop @res if (@res > 1);
	} elsif ($x ne '.') {
	    push @res, $x;
	}
    }
    ($pn, join ($pdel, @res))
}
