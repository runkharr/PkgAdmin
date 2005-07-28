#! /usr/bin/perl
#
# $Id: hgen.pl,v 1.1.1.1 2005-07-28 14:16:37 bj Exp $
#
# Author: Boris Jakubith
# E-Mail: bj@isv-gmbh.de
# Copyright: 2005, Boris Jakubith <bj@isv-gmbh.de>
# Licence: GPL(v2)

## Usage: hgen.pl -o outfile template headerfile...

sub usage ();

($prog = $0) =~ s(.*/)();

while (@ARGV && ($_ = $ARGV[0], /^-/)) {
    shift;
    last if (/^--$/);
    if (/^-h$|^-help$|^--help$/) {
	usage ();
    } elsif (/^-o(.*)|^--output=(.+)|^--output$/) {
	unless ($1 or @ARGV) {
	    die "$prog: missing argument for option `-o'.\n";
	}
	$outfile = (($1) ? ($1) : (shift));
    } else {
	die "$prog: invalid option `$_'.\n";
    }
}

if (@ARGV < 2) { die "$prog: missing arguments.\n"; }

$tmpl = (shift);

if ($outfile) {
    open OUT, ">$outfile" or
	die "$prog: attempt to write `$outfile' failed - $!\n";
} else {
    open OUT, ">&STDOUT" or
	die "$prog: attempt to write to `STDOUT' failed - $!\n";
}

select OUT; $| = 1; select STDOUT;
open TMPL, "<$tmpl" or
    die "$prog: attempt to read `$tmpl' failed - $!\n";

%HDRS = map { $_ => 1 } @ARGV;

while (<TMPL>) {
    if (/^#include "([^"]+)"/) {
	$f = $1; $sw = 0;
	unless (exists $HDRS{$f}) {
	    print OUT $_;
	} elsif (open HDR, "<$f") {
	    $in = ''; $lc = 0;
	    do {
		while (defined ($in = <HDR>)) {
		    ++$lc; $x = $in; chomp $x; $x =~ s/\r$//;
		    if ($x eq '/*##EXPORT##*/') {
			print OUT "/* From: $f ($lc) */\n"; $sw = 1; last;
		    }
		}
		while (defined ($in = <HDR>)) {
		    ++$lc; $x = $in; chomp $x; $x =~ s/\r$//;
		    if ($x eq '/*##END##*/') {
			print OUT "/* End $f ($lc) */\n"; $sw = 0; last;
		    }
		    print OUT $in;
		}
	    } while (defined $in);
	    print OUT "/* End $f ($lc) */\n" if ($sw);
	    close HDR;
	    print OUT "\n";
	} else {
	    print STDERR "$prog: opening `$f' failed - `#include' ".
			 "remains unchanged.\n";
	    print OUT $_;
	}
    } elsif (/^#define ([A-Za-z][a-zA-Z0-9]+_[Vv][Ee][Rr][Ss][Ii][Oo][Nn])/) {
	$vfcname = $1;
	unless (open VERSION, "<VERSION") {
	    print "$prog: attempt to generate the $vcfname macro (function) ".
		  "failed - $!\n";
	    print "$prog: leaving the empty macro unchanged ...\n";
	    print OUT $_;
	} else {
	    $_ = <VERSION>; chomp; s/\r$//; close VERSION;
	    print OUT "#define $vfcname() \"$_\"\n";
	}
    } else {
	print OUT $_;
    }
}
close TMPL;
close OUT;
exit 0;

sub usage () {
    die <<EOT;
Usage: $prog [-o outfile] template header ...
       $prog -h

  Options:
    -o (alt: --output)
      write the output to the specified file instead to `STDOUT'

  Arguments:
    template
      the template header file which is the base of the produced output.
      All of the files `header' which have a corresponding `#include'-line
      will be inserted into the output - thereby replacing the corresponding
      `#include'-line.
    header
      a header file to be inserted; only sections between `/*##EXPORT##*/'
      and a corresponding `/*##END##*/' will be inserted into the output.

EOT
}
