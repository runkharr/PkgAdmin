#! /usr/bin/perl

use Cwd;

($prog = $0) =~ s(.*/)();
($path = $0) =~ s((/|)[^/]+$)();

if ($path eq '') { $path = (cwd); }

unless (@ARGV) {
    print STDERR "Usage: $prog <file>\n"; exit 64;
}

$file = (shift);

if (system 'cc', "-o$path/itypes.bin", "$path/itypes.c") {
    print STDERR "$prog: Compilation failed - $!\n"; exit 71;
}

if ($pid = open FILE, '-|') {
    while (<FILE>) { print $_; }
    close FILE;
} elsif (defined $pid) {
    open STDIN, "<$file" or die "$prog: $file - $!\n";
    exec "$path/itypes.bin"; die "$prog: $!\n";
} else {
    print STDERR "$prog: fork () failed - $!\n"; exit 71;
}

unlink "$path/itypes.bin";
exit 0;
