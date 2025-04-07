/* 
 * Copyright (c) 2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * static int
 * wrap_ftw64(const char *path, int (*fn)(const char *, const struct stat64 *, int), int nopenfd) {
 *	int rc = -1;
 */
    typedef int (*pseudo_nftw64_func_t) (const char *filename,
                                        const struct stat64 *status, int flag,
                                        struct FTW *info);

    // 1. Set the flag argument to 0, just like glibc does.
    // 2. The difference between ftw and nftw callback
    //    is only the last parameter: struct FTW is only used
    //    by nftw(), and it is missing from ftw().
    //    However since otherwise the stacklayout for the
    //    functions is the same, this cast should work just the
    //    way we want it. This is also borrowed from glibc.

    // This, of course in turn will be captured by pseudo, and will be passed
    // to the respective wrapper of nftw64()
    rc = nftw64(path, (pseudo_nftw64_func_t)fn, nopenfd, 0);

/*	return rc;
 * }
 */
