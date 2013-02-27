#! /usr/bin/env make -f
#
# $Id$
#
# Author: Boris Jakubith
# E-Mail: fbj@blinx.de
# Copyright: (c) 2009, Boris Jakubith <fbj@blinx.de>
# License: GPL (version 2)
#
# Small "cleanup"-Makefile for the admin directory ...
#

CLEAN_LIST = cgen.bin exclude_list.bin distfile.bin run uninstall.bin a.out core

clean:
	@echo -n "Cleaning up admin ..."
	@rm -f $(CLEAN_LIST)
	@echo " done."
