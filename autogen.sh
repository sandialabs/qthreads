#!/bin/bash
#
# Authors: Brian W. Barrett <bwbarre@sandia.gov>,
#          Kyle B. Wheeler <kbwheel@sandia.gov>
#

echo "Generating configure files..."

if [ "$LIBTOOL" ] ; then
	# accept anything that's been pre-configured
	echo Using LIBTOOL=$LIBTOOL
elif type glibtool &>/dev/null ; then
	# prefer glibtool over libtool
	export LIBTOOL=`type -p glibtool`
elif type libtool &>/dev/null ; then
	export LIBTOOL=`type -p libtool`
else
	echo "ERROR!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
	echo "I need libtool in order to generate the configure script and makefiles. I couldn't find either libtool or glibtool in your PATH. Perhaps you need to set the LIBTOOL environment variable to point toward a custom installation?"
	exit -1
fi

if [ "$AUTOMAKE" ] ; then
	# accept anything that's been pre-configured
	echo Using AUTOMAKE=$AUTOMAKE
elif type automake &>/dev/null ; then
	export AUTOMAKE=`type -p automake`
else
	echo "ERROR!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
	echo "I need automake in order to generate the configure script and makefiles. I couldn't find it in your PATH, though. Perhaps you need to set the AUTOMAKE environment variable to point toward a custom installation?"
	exit -1
fi

if [ "$AUTOCONF" ] ; then
	# accept anything that's been pre-configured
	echo Using AUTOCONF=$AUTOCONF
elif type autoconf &>/dev/null ; then
	export AUTOCONF=`type -p autoconf`
else
	echo "ERROR!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
	echo "I need autoconf in order to generate the configure script and makefiles. I couldn't find it in your PATH, though. Perhaps you need to set the AUTOCONF environment variable to point toward a custom installation?"
	exit -1
fi

if [ "$AUTORECONF" ] ; then
	# accept anything that's been pre-configured
	echo Using AUTORECONF=$AUTORECONF
elif type autoreconf &>/dev/null ; then
	export AUTORECONF=`type -p autoreconf`
else
	echo "ERROR!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
	echo "I need autoreconf in order to generate the configure script and makefiles. I couldn't find it in your PATH, though. Perhaps you need to set the AUTORECONF environment variable to point toward a custom installation?"
	exit -1
fi

# If this directory isn't removed, the configure script may not have the right
# dynamically-generated version number
if [ -d autom4te.cache ] ; then
	rm -rf autom4te.cache
fi

version=$(awk '{if(NR==1)print$2;else exit}' ./NEWS)
if [[ ${version%b} != ${version} && $SKIPVGEN != 1 ]] ; then
	echo "Querying svn to determine revision number..."
	svn stat -u README | tee .autogen_svn_output
	rev=$(awk '/^Status against revision: /{printf "'$version'-%s", $4}' .autogen_svn_output )
	if [ "$rev" ] ; then
		echo -n $rev > .autogen-version
	else
		echo $version | tr -d '\012' > .autogen-version
	fi
else
	echo $version | tr -d '\012' > .autogen-version
fi

$AUTORECONF --install --symlink --warnings=gnu,obsolete,override,portability,no-obsolete "$@" && \
  echo "Preparing was successful if there were no error messages above." && \
  exit 0

echo "It appears that configure file generation failed.  Sorry :(."
exit -1
