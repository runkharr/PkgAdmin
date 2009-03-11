#! /usr/bin/env make -f
#
# $Id: clean.mk,v 1.2 2009-03-11 17:29:58 bj Exp $
#
# Author: Boris Jakubith
# E-Mail: fbj@blinx.de
# Copyright: (c) 2009, Boris Jakubith <fbj@blinx.de>
# License: GPL (version 2)
#
# Small "cleanup"-Makefile for the admin directory ...
#

CLEAN_LIST = exclude_list.bin

clean:
	@echo "Cleaning up admin"
	@rm -f $(CLEAN_LIST)
