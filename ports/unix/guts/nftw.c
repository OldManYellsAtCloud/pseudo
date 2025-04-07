/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * static int
 * wrap_nftw(const char *path, int (*fn)(const char *, const struct stat *, int, struct FTW *), int nopenfd, int flag) {
 *	int rc = -1;
 */

/***********************************
 * This call has a custom wrapper
 * where all the logic happens just
 * around this single line.
 ***********************************/
    rc = real_nftw(path, NFTW_CALLBACK_NAME, nopenfd, flag);

/*	return rc;
 * }
 */
