#! /usr/bin/perl
#
# $Id: deb-prepare.pl,v 1.1 2009-02-24 18:33:40 bj Exp $
#
# Author: Boris Jakubith
# E-Mail: fbj@blinx.de
# Copyright: (c) 2009, Boris Jakubith <fbj@blinx.de>
# License: GPL (version 2)
#
# Small script for perparing a debian package generation ...
#

use File::Copy;

sub ppsplit ($;$);
sub files_exist ($$@);

($prog, $ppath) = ppsplit ($0);
$fill = (' ' x (length $prog));

unless (-d 'admin' && -x "admin/$prog") {
    die "$prog: This script must be called from the top-level directory of\n".
	"$fill  the package source-tree.\n";
}

unless (-d 'debian') {
    $ok = files_exist ('admin/debian', 'changelog', 'compat', 'control',
		       'copyright', 'rules')
    unless ($ok) {
	die "$prog: neither a 'debian' sub-directory nor the debian package-".
	    "generation\n$fill  templates were found.\n";
    }
    unless (mkdir ('debian')) {
	die "$prog: attempt to »mkdir 'debian'« failed - $!\n";
    }
    for $file ('changelog', 'compat', 'control', 'copyright', 'rules') {
	copy ("admin/debian/$file", 'debian');
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

sub files_exist ($$@) {
    my ($dir, @files) = @_;
    unless (-d $dir) {
	undef
    } else {
	my $rf = 1;
	for my $f in (@files) {
	    if (-f "$dir/$f") { $rf = 0; last; }
	}
	$rf
    }
}
