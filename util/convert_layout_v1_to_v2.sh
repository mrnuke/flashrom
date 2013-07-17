#!/bin/sh
#
#  Copyright 2013 Stefan Tauner
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
#  MA 02110-1301, USA.
#
#
#
# This script converts legacy layout files as understood by flashrom up to version 0.9.7 to format version 2.0.
# It converts all files given as parameters in place and creates backups (with the suffix ".old") of the old
# contents unless --nobackup is given.
#
# It does...
# - check if the file format is already in 2 format
# - prefix addresses with 0x if they have not been already
# - remove superfluous white space (i.e. more than one consecutive space)
# - remove white space from otherwise empty lines

usage ()
{
	echo "Usage: $0 [--nobackup] FILE..."
	exit 1
}

if [ $# -eq 0 ]; then
	usage
fi

if [ $1 = "--nobackup" ]; then
	sed_opt="-i"
	shift
	if [ $# -eq 0 ]; then
		usage
	fi
else
	sed_opt="-i.old"
fi

for f in "$@" ; do
	if [ ! -e "$f" ]; then
		echo "File not found: $f">&2
		continue
	fi

	if grep -q 'flashrom layout 2\b' "$f" ; then
		echo "File already in new format: $f"
		continue
	fi

	sed $sed_opt -e "
		1i # flashrom layout 2
		s/ *\(0x\|\)\([0-9a-fA-F][0-9a-fA-F]*\) *: *\(0x\|\)\([0-9a-fA-F][0-9a-fA-F]*\) */0x\2:0x\4 /
		s/   */ /
		s/^ *$//
		" "$f"
	echo "$f done"
done
echo "Done!"
