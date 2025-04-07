/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * static int
 * wrap_nftw64(const char *path, int (*fn)(const char *, const struct stat64 *, int, struct FTW *), int nopenfd, int flag) {
 *	int rc = -1;
 */

/***********************************
 * This call has a custom wrapper
 * where all the logic happens just
 * around this single line.
 ***********************************/
    rc = real_nftw64(path, NFTW_CALLBACK_NAME, nopenfd, flag);

/*	return rc;
 * }
 */
