#!/bin/bash
#
# Authors: Brian W. Barrett <bwbarre@sandia.gov>,
#          Kyle B. Wheeler <kbwheel@sandia.gov>
#

echo "Generating configure files..."

# prefer glibtool over libtool
if type glibtool &>/dev/null ; then
	export LIBTOOL=`type -p glibtool`
fi

# If this directory isn't removed, the configure script may not have the right
# dynamically-generated version number
if [ -d autom4te.cache ] ; then
	rm -rf autom4te.cache
fi

version=$(awk '{if(NR==1)print$2;else exit}' ./NEWS)
if [[ ${version%b} != ${version} && $SKIPVGEN != 1 ]] ; then
	echo "Querying svn to determine revision number..."
	svn stat -u README
	rev=$(svn stat -u README 2>/dev/null | awk '/^Status against revision: /{printf "'$version'-%s", $4}')
	if [ "$rev" ] ; then
		echo -n $rev > .autogen-version
	else
		echo $version | tr -d '\012' > .autogen-version
	fi
else
	echo $version | tr -d '\012' > .autogen-version
fi

autoreconf --install --symlink --warnings=gnu,obsolete,override,portability,no-obsolete "$@" && \
  echo "Preparing was successful if there were no error messages above." && \
  exit 0

echo "It appears that configure file generation failed.  Sorry :(."
exit 1
