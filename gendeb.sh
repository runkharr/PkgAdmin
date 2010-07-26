#! /bin/bash
#
# $Id$
#
# Author: Boris Jakubith
# E-Mail: bj@isv-gmbh.de
# Copyright: © 2005, Boris Jakubith <bj@isv-gmbh.de>
# Licence: GPL(v2)

# Get the last version number of the `changelog' file ...
#
DV=`head -1 admin/debian/changelog|\
    sed -e 's|[^(]*(\([0-9]\+\.[0-9.]\+\)[^)]*).*|\1|'`
#
# and the current package version ...
#
PV=`cat VERSION`

# Check if both version numbers differ ...
#
if [ "$DV" != "$PV" ]; then
    # If so, then insert a new entry into the `debian/changelog'-directory ...
    #
    # ... generate a new debian version number ...
    DV=`head -1 admin/debian/changelog|\
	sed -e "s|.*(\([0-9]\+\.[0-9]\+\)\(.*\)).*|$PV\2|"`

    # ... generate the date for the `AUTHOR  DATE'-entry ...
    DT=`unset LANG LC_CTYPE LC_ALL; date -R`

    # ... generate the new entry ...
    cat <<-EOT >admin/debian/changelog.new
	rcheckrcpt ($DV) unstable; urgency=low

	  * New upstream release.
	
	 -- Boris Jakubith <bj@isv-gmbh.de>  $DT
	
	EOT
    # ... append the existing `debian/changelog'-file ...
    cat admin/debian/changelog >>admin/debian/changelog.new
    # ... copy back to `debian/changelog' ...
    cat admin/debian/changelog.new >admin/debian/changelog
    # ... remove the temporary file ...
    rm -f admin/debian/changelog.new
fi

if [ $# -gt 0 -a "$1" = changelog ]; then exit 0; fi

# Generate the directory for the `Debian'-source ...
#
mkdir -m 0755 'debdir';

# Copy the required files into it ...
#
find . -name 'CVS' -prune -o -path '*/CVS/*' -prune -o -name 'debdir' -prune \
       -o -path 'debdir/*' -prune -o -print \
| cpio -pdvm debdir

# Change into the `Debian' source directory ...
#
cd 'debdir'

# Move the `debian' sub-directory from `admin/' to here ...
#
cp -a ../admin/debian .
../admin/dcedit

# Use `dpkg-buildpackage' in order to generate the debian packages ...
#
dpkg-buildpackage -rfakeroot -b -us -uc

# Back to parent directory and remove the `Debian' source directory ...
#
cd ..; rm -rf debdir

# Last not least, move the `.deb'-files outside of this directory ...
#
mv *.deb *.changes ..

exit 0
