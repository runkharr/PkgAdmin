#! /usr/bin/perl
#
# $Id: getconf.pl,v 1.2 2005-11-22 08:05:20 bj Exp $
#
# Author: Boris Jakubith
# E-Mail: bj@isv-gmbh.de
# Copyright: (c) 2005, Boris Jakubith <bj@isv-gmbh.de>
# Licence: GPLv2

($prog = $0) =~ s(.*/)();

if (@ARGV < 2) { print STDERR "Usage: $prog <file> <ident>\n"; exit 64; }

$file = (shift);
$name = (shift);

unless (open FILE, "<$file") {
    print STDERR "$prog: attempt to open `$file' failed - $!\n"; exit 66;
}
@lines = grep /^#define\s+\Q$name\E(\s+(.+))?$/, <FILE>;
close FILE;

if (@lines < 1) {
    print STDERR "$prog: `$name' not found in `$file'!\n"; exit 65;
}

if (@lines > 1) {
    print STDERR "$prog: ambigeous `$name' in `$file'!\n"; exit 65;
}

($cfval = $lines[$[]) =~ s/^#define\s\Q$name\E\s*//;

if ($cfval =~ /\/$/) {
    print STDERR "$prog: invalid `$name'-configuration in `$file'!\n"; exit 65;
}

if ($cfval =~ /^[0-9]/) {
    $cfval =~ s/\s+.*$//;
} elsif ($cfval =~ /^"/) {
    @cfval = split //, (substr $cfval, 1); $ix = $[;
    while ($cfval[$ix] ne '"') {
	++$ix if ($cfval[$ix] eq '\\');
	++$ix;
    }
    $cfval = join ('', @cfval[$[ .. $ix - 1]);
    @cfval = ();
}

print "$cfval\n";
exit 0;
