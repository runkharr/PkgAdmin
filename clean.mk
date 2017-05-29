#! /usr/bin/make -f
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

CLEAN_LIST = $(shell echo *.bin) *.o run a.out core

PROG=$(lastword $(MAKEFILE_LIST))
DIR="$(shell dirname "$(PROG)")"


clean:
	@echo -n "Cleaning up $(DIR) ..."
	@cd "$(DIR)"; rm -f $(CLEAN_LIST)
	@echo " done."
