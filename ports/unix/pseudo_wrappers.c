/*
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 */

/**********************************************
 * POPEN
 **********************************************/
FILE *
popen(const char *command, const char *mode) {
	sigset_t saved;
	
	FILE *rc = NULL;

	if (!pseudo_check_wrappers() || !real_popen) {
		/* rc was initialized to the "failure" value */
		pseudo_enosys("popen");
		return rc;
	}

	pseudo_debug(PDBGF_WRAPPER, "called: popen\n");
	pseudo_sigblock(&saved);
	if (pseudo_getlock()) {
		errno = EBUSY;
		sigprocmask(SIG_SETMASK, &saved, NULL);
		return NULL;
	}

	int save_errno;
	/* exec*() use this to restore the sig mask */
	pseudo_saved_sigmask = saved;
	rc = wrap_popen(command, mode);
	
	save_errno = errno;
	pseudo_droplock();
	sigprocmask(SIG_SETMASK, &saved, NULL);
#if 0
/* This can cause hangs on some recentish systems which use locale
 * stuff for strerror...
 */
	pseudo_debug(PDBGF_WRAPPER, "completed: popen (maybe: %s)\n", strerror(save_errno));
#endif
	pseudo_debug(PDBGF_WRAPPER, "completed: popen (errno: %d)\n", save_errno);
	errno = save_errno;
	return rc;
}

static FILE *
wrap_popen(const char *command, const char *mode) {
	FILE *rc = NULL;
	
	

#include "guts/popen.c"

	return rc;
}


/**********************************************
 * NFTW
 **********************************************/

#define CONCAT_EXPANDED(prefix, value) prefix ## value
#define CONCAT(prefix, value) CONCAT_EXPANDED(prefix, value)

#undef NFTW_NAME
#undef NFTW_WRAP_NAME
#undef NFTW_REAL_NAME
#undef NFTW_STAT_STRUCT
#undef NFTW_STAT_NAME
#undef NFTW_LSTAT_NAME
#undef NFTW_GUTS_INCLUDE
#undef NFTW_CALLBACK_NAME
#undef NFTW_STORAGE_STRUCT_NAME
#undef NFTW_STORAGE_ARRAY_SIZE
#undef NFTW_MUTEX_NAME
#undef NFTW_STORAGE_ARRAY_NAME
#undef NFTW_APPEND_FN_NAME
#undef NFTW_DELETE_FN_NAME
#undef NFTW_FIND_FN_NAME

#define NFTW_NAME nftw
#define NFTW_WRAP_NAME CONCAT(wrap_, NFTW_NAME)
#define NFTW_REAL_NAME CONCAT(real_, NFTW_NAME)
#define NFTW_STAT_NAME stat
#define NFTW_STAT_STRUCT stat
#define NFTW_LSTAT_NAME lstat
#define NFTW_CALLBACK_NAME CONCAT(wrap_callback_, NFTW_NAME)
#define NFTW_GUTS_INCLUDE "nftw.c"
#define NFTW_STORAGE_STRUCT_NAME CONCAT(storage_struct_, NFTW_NAME)
#define NFTW_STORAGE_ARRAY_SIZE CONCAT(storage_size_, NFTW_NAME)
#define NFTW_MUTEX_NAME CONCAT(mutex_, NFTW_NAME)
#define NFTW_STORAGE_ARRAY_NAME CONCAT(storage_array_, NFTW_NAME)
#define NFTW_APPEND_FN_NAME CONCAT(append_to_array_, NFTW_NAME)
#define NFTW_DELETE_FN_NAME CONCAT(delete_from_array_, NFTW_NAME)
#define NFTW_FIND_FN_NAME CONCAT(find_in_array_, NFTW_NAME)

#include "guts/nftw_wrapper_base.c"
