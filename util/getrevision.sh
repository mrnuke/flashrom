#!/bin/sh
#
# This file is part of the flashrom project.
#
# Copyright (C) 2005 coresystems GmbH <stepan@coresystems.de>
# Copyright (C) 2009,2010 Carl-Daniel Hailfinger
# Copyright (C) 2010 Chromium OS Authors
# Copyright (C) 2013 Stefan Tauner
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
#

EXIT_SUCCESS=0
EXIT_FAILURE=1
date_format="+%Y-%m-%dT%H:%M:%S%z" # There is only one valid timeformat FFS! ISO 8601

svn_revision() {
	LC_ALL=C svnversion -cn . 2>/dev/null | \
		sed -e "s/.*://" -e "s/\([0-9]*\).*/r\1/" | \
		grep "r[0-9]" ||
	LC_ALL=C svn info . 2>/dev/null | \
		grep "Last Changed Rev:" | \
		sed -e "s/^Last Changed Rev: *//" -e "s/\([0-9]*\).*/r\1/" | \
		grep "r[0-9]" ||
	echo "unknown"
}

svn_url() {
	echo $(LC_ALL=C svn info 2>/dev/null |
	       grep URL: |
		   sed 's/.*URL:[[:blank:]]*//' |
		   grep ^.
	      )
}

svn_has_local_changes() {
	svn status | egrep '^ *[ADMR] *' > /dev/null
}

svn_timestamp() {
	local timestamp

	if svn_has_local_changes ; then
		timestamp=$(date "${date_format}")
	else
		# No local changes, get date of the last log record.
		local last_commit_date=$(svn info | grep '^Last Changed Date:' | \
		                         awk '{print $4" "$5" "$6}')
		timestamp=$(date -d "${last_commit_date}" "${date_format}")
	fi

	echo "${timestamp}"
}

# Retrieve svn revision using git log data (for git-svn mirrors)
gitsvn_revision() {
	local r

	# If this is a "native" git-svn clone we could use git svn log like so
	# if [ -e .git/svn/.metadata ]; then
	# 	r=$(git svn log --oneline -1 | sed 's/^r//;s/[[:blank:]].*//')
	# else
		r=$(git log --grep git-svn-id -1 | \
			grep git-svn-id | \
			sed 's/.*@/r/;s/[[:blank:]].*//')
	# fi

	echo "${r}"
}

git_has_local_changes() {
	git update-index -q --refresh
	! git diff-index --quiet HEAD --
}

git_timestamp() {
	local timestamp

	# are there local changes?
	if git_has_local_changes ; then
		timestamp=$(date "${date_format}")
	else
		# No local changes, get date of the last commit
		timestamp=$(date -d "$(git log --pretty=format:"%cD" -1)" "${date_format}")
	fi

	echo "${timestamp}"
}

git_url() {
	# get all remote branches containing the last commit (excluding 'origin/HEAD -> origin/master')
	branches=$(git branch -r --contains HEAD | sed 's/[\t ]*//;/.*->.*/d')
	if [ -z "$branches" ] ; then
		echo "No remote branch contains current HEAD">&2
		return
	fi

	# find "nearest" branch
	local diff=9000
	local target=
	for branch in $branches ; do
		curdiff=$(git rev-list --count HEAD..$branch)
		if [ $curdiff -ge $diff ] ; then
			continue
		fi
		diff=$curdiff
		target=$branch
	done

	echo "$(git ls-remote --exit-code --get-url ${target%/*}) ${target#*/}"
}

scm_url() {
	local url

	if [ -d ".svn" ] ; then
		url=$(svn_url)
	elif [ -d ".git" ] ; then
		url=$(git_url)
	fi

	echo "${url}"
}

# Retrieve timestamp since last modification. If the sources are pristine,
# then the timestamp will match that of the SCM's more recent modification
# date.
timestamp() {
	local t

	if [ -d ".svn" ] ; then
		t=$(svn_timestamp)
	elif [ -d ".git" ] ; then
		t=$(git_timestamp)
	fi

	echo ${t}
}

# Retrieve local SCM revision info. This is useful if we're working in a different SCM than upstream and/or
# have local changes.
local_revision() {
	local r

	if [ -d ".svn" ] ; then
		r=$(svn_has_local_changes && echo "-dirty")
	elif [ -d ".git" ] ; then
		r=$(git rev-parse --short HEAD)

		local svn_base=$(git log --grep git-svn-id -1 --format='%h')
		if [ "$svn_base" != "" ] ; then
			r="$r-$(git rev-list --count $svn_base..HEAD)"
		fi

		if git_has_local_changes ; then
			r="$r-dirty"
		fi
	fi

	echo ${r}
}

# Get the upstream flashrom revision stored in SVN metadata.
#
# If the local copy is svn, then use svnversion
# If the local copy is git, then scrape upstream revision from git logs
upstream_revision() {
	local r

	if [ -d ".svn" ] ; then
		r=$(svn_revision)
	elif [ -d ".git" ] ; then
		r=$(gitsvn_revision)
	else
		r="unknown"
	fi

	echo "${r}"
}

show_help() {
	echo "Usage:
	${0} <option>

Options
    -h or --help
        Display this message.
    -l or --local
        local revision (if different from upstream) and an indicator for uncommitted changes
    -u or --upstream
        upstream flashrom revision
    -U or --url
        url associated with local copy of flashrom
    -t or --timestamp
        timestamp of most recent modification
	"
	return
}

if [ -z "${1}" ] ; then
	show_help;
	echo "No options specified";
	exit ${EXIT_FAILURE}
fi

# The is the main loop
while [ $# -gt 0 ];
do
	case ${1} in
	-h|--help)
		show_help;
		shift;;
	-l|--local)
		echo "$(local_revision)";
		shift;;
	-u|--upstream)
		echo "$(upstream_revision)";
		shift;;
	-U|--url)
		echo "$(scm_url)";
		shift;;
	-t|--timestamp)
		echo "$(timestamp)";
		shift;;
	-*)
		show_help;
		echo "invalid option: ${1}"
		exit ${EXIT_FAILURE};;
	*)
		shift;; # ignore arguments not starting with -
	esac;
done

exit ${EXIT_SUCCESS}
