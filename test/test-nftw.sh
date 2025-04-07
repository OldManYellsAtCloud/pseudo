#!/bin/bash
#
# Test nftw call and its behavior modifying flags
# SPDX-License-Identifier: LGPL-2.1-only
#

trap "rm -rf ./walking" 0

check_retval_and_fail_if_needed(){
  if [ $1 -ne 0 ]; then
    echo $2
    exit 1
  fi
}


mkdir -p walking/a1/b1/c1
touch walking/a1/b1/c1/file
mkdir walking/a1/b2
mkdir walking/a1/b3
touch walking/a1/b1/c1/file2
touch walking/a1/b1/c1/file3
touch walking/a1/b2/file4
touch walking/a1/b2/file5

./test/test-nftw skip_subtree_pseudo
check_retval_and_fail_if_needed $? "nftw subtree skipping with pseudo failed"

./test/test-nftw skip_siblings_pseudo
check_retval_and_fail_if_needed $? "nftw sibling skipping with pseudo failed"

./test/test-nftw skip_siblings_chdir_pseudo
check_retval_and_fail_if_needed $? "nftw sibling skipping chddir with pseudo failed"

./test/test-nftw64 skip_subtree_pseudo
check_retval_and_fail_if_needed $? "nftw64 subtree skipping with pseudo failed"

./test/test-nftw64 skip_siblings_pseudo
check_retval_and_fail_if_needed $? "nftw64 sibling skipping with pseudo failed"

./test/test-ftw pseudo_no_recursion
check_retval_and_fail_if_needed $? "ftw non-recursive walking with pseudo failed"

./test/test-ftw pseudo_recursion
check_retval_and_fail_if_needed $? "ftw recursive walking with pseudo failed"

./test/test-ftw64 pseudo_no_recursion
check_retval_and_fail_if_needed $? "ftw64 non-recursive walking with pseudo failed"

./test/test-ftw64 pseudo_recursion
check_retval_and_fail_if_needed $? "ftw64 recursive walking with pseudo failed"


export PSEUDO_DISABLED=1

uid=`env -i id -u`
gid=`env -i id -g`

./test/test-nftw skip_subtree_no_pseudo $gid $uid
check_retval_and_fail_if_needed $? "nftw subtree skipping without pseudo failed"

./test/test-nftw skip_siblings_no_pseudo $gid $uid
check_retval_and_fail_if_needed $? "nftw sibling skipping without pseudo failed"

./test/test-nftw skip_siblings_chdir_no_pseudo $gid $uid
check_retval_and_fail_if_needed $? "nftw sibling skipping chdir without pseudo failed"

./test/test-nftw64 skip_subtree_no_pseudo $gid $uid
check_retval_and_fail_if_needed $? "nftw subtree skipping without pseudo failed"

./test/test-nftw64 skip_siblings_no_pseudo $gid $uid
check_retval_and_fail_if_needed $? "nftw sibling skipping without pseudo failed"

./test/test-ftw no_pseudo_no_recursion $gid $uid
check_retval_and_fail_if_needed $? "ftw non-recursive walking without pseudo failed"

./test/test-ftw no_pseudo_recursion $gid $uid
check_retval_and_fail_if_needed $? "ftw recursive walking without pseudo failed"

./test/test-ftw64 no_pseudo_no_recursion $gid $uid
check_retval_and_fail_if_needed $? "ftw non-recursive walking without pseudo failed"

./test/test-ftw64 no_pseudo_recursion $gid $uid
check_retval_and_fail_if_needed $? "ftw recursive walking without pseudo failed"
