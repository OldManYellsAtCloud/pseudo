#!/bin/bash
#
# Test nftw call and its behavior modifying flags
# SPDX-License-Identifier: LGPL-2.1-only
#

trap "rm -rf ./walking" 0

mkdir -p walking/a1/b1/c1
touch walking/a1/b1/c1/file

./test/test-nftw-walking normal_folder
if [ $? -ne 0 ]; then
  echo "nftw folder walking failed"
  exit 1
fi

./test/test-nftw-walking depth_folder
if [ $? -ne 0 ]; then
  echo "nftw depth folder walking failed"
  exit 1
fi

./test/test-nftw-walking normal_file
if [ $? -ne 0 ]; then
  echo "nftw file walking failed"
  exit 1
fi

./test/test-nftw-walking depth_file
if [ $? -ne 0 ]; then
  echo "nftw depth file walking failed"
  exit 1
fi
