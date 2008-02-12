#!/bin/sh
#
# Author: Brian W. Barrett <bwbarre@sandia.gov>
#

echo "Generating configure files..."

autoreconf --install --symlink --warnings=all,no-obsolete && \
  echo "Preparing was successful if there was no error messages above." && \
  exit 0

echo "It appears that configure file generation failed.  Sorry :(."
