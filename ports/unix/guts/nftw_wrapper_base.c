int
NFTW_NAME(const char *path, int (*fn)(const char *, const struct NFTW_STAT_STRUCT *, int, struct FTW *), int nopenfd, int flag) {
        sigset_t saved;

        int rc = -1;

        if (!pseudo_check_wrappers() || !NFTW_REAL_NAME) {
                /* rc was initialized to the "failure" value */
                pseudo_enosys("nftw");
                return rc;
        }

        if (pseudo_disabled) {
                rc = (*NFTW_REAL_NAME)(path, fn, nopenfd, flag);

                return rc;
        }

        pseudo_debug(PDBGF_WRAPPER, "wrapper called: %s\n", __func__);
        pseudo_sigblock(&saved);
        pseudo_debug(PDBGF_WRAPPER | PDBGF_VERBOSE, "%s - signals blocked, obtaining lock\n", __func__);
        if (pseudo_getlock()) {
                errno = EBUSY;
                sigprocmask(SIG_SETMASK, &saved, NULL);
                pseudo_debug(PDBGF_WRAPPER, "%s failed to get lock, giving EBUSY.\n", __func__);
                return -1;
        }

        int save_errno;
        if (antimagic > 0) {
                /* call the real syscall */
                pseudo_debug(PDBGF_SYSCALL, "%s calling real syscall.\n", __func__);
                rc = (*NFTW_REAL_NAME)(path, fn, nopenfd, flag);
        } else {
                path = pseudo_root_path(__func__, __LINE__, AT_FDCWD, path, 0);
                if (pseudo_client_ignore_path(path)) {
                        /* call the real syscall */
                        pseudo_debug(PDBGF_SYSCALL, "%s ignored path, calling real syscall.\n", __func__);
                        rc = (*NFTW_REAL_NAME)(path, fn, nopenfd, flag);
                } else {
                        /* exec*() use this to restore the sig mask */
                        pseudo_saved_sigmask = saved;
                        rc = NFTW_WRAP_NAME(path, fn, nopenfd, flag);
                }
        }

        save_errno = errno;
        pseudo_droplock();
        sigprocmask(SIG_SETMASK, &saved, NULL);
        pseudo_debug(PDBGF_WRAPPER | PDBGF_VERBOSE, "%s - yielded lock, restored signals\n", __func__);
        pseudo_debug(PDBGF_WRAPPER, "wrapper completed: %s returns %d (errno: %d)\n", __func__, rc, save_errno);
        errno = save_errno;
        return rc;
}

struct NFTW_STORAGE_STRUCT_NAME {
    int (*callback)(const char *, const struct NFTW_STAT_STRUCT *, int, struct FTW *);
    int flags;
    pthread_t tid;
};

static struct NFTW_STORAGE_STRUCT_NAME *NFTW_STORAGE_ARRAY_NAME;
size_t NFTW_STORAGE_ARRAY_SIZE = 0;
static pthread_mutex_t NFTW_MUTEX_NAME = PTHREAD_MUTEX_INITIALIZER;

static void NFTW_APPEND_FN_NAME(struct NFTW_STORAGE_STRUCT_NAME *data_to_append){
    NFTW_STORAGE_ARRAY_NAME = realloc(NFTW_STORAGE_ARRAY_NAME, ++NFTW_STORAGE_ARRAY_SIZE * sizeof(*data_to_append));
    memcpy(&NFTW_STORAGE_ARRAY_NAME[NFTW_STORAGE_ARRAY_SIZE - 1], data_to_append, sizeof(*data_to_append));
}

int NFTW_FIND_FN_NAME(struct NFTW_STORAGE_STRUCT_NAME* target) {
    pthread_t tid = pthread_self();

    // return the last one, not the first
    for (ssize_t i = NFTW_STORAGE_ARRAY_SIZE - 1; i >= 0; --i){
        if (NFTW_STORAGE_ARRAY_NAME[i].tid == tid){
            // need to dereference it, as next time this array
            // may be realloc'd, making the original pointer
            // invalid
            *target = NFTW_STORAGE_ARRAY_NAME[i];
            return 1;
        }
    }

    return 0;
}

static void NFTW_DELETE_FN_NAME() {
    pthread_t tid = pthread_self();

    if (NFTW_STORAGE_ARRAY_SIZE == 1) {
        if (NFTW_STORAGE_ARRAY_NAME[0].tid == tid) {
            free(NFTW_STORAGE_ARRAY_NAME);
            NFTW_STORAGE_ARRAY_NAME = NULL;
            --NFTW_STORAGE_ARRAY_SIZE;
        } else {
            pseudo_diag("%s: Invalid callback storage content, can't find corresponding data", __func__);
        }
        return;
    }

    int found_idx = -1;
    for (ssize_t i = NFTW_STORAGE_ARRAY_SIZE - 1; i >= 0; --i) {
        if (NFTW_STORAGE_ARRAY_NAME[i].tid == tid) {
            found_idx = i;
            break;
        }
    }

    if (found_idx == -1) {
        pseudo_diag("%s: Invalid callback storage content, can't find corresponding data", __func__);
        return;
    }

    // delete the item we just found
    for (size_t i = found_idx + 1; i < NFTW_STORAGE_ARRAY_SIZE; ++i)
        NFTW_STORAGE_ARRAY_NAME[i - 1] = NFTW_STORAGE_ARRAY_NAME[i];

    NFTW_STORAGE_ARRAY_NAME = realloc(NFTW_STORAGE_ARRAY_NAME, --NFTW_STORAGE_ARRAY_SIZE * sizeof(struct NFTW_STORAGE_STRUCT_NAME));

}

static int NFTW_CALLBACK_NAME(const char* fpath, const struct NFTW_STAT_STRUCT *sb, int typeflag, struct FTW *ftwbuf) {
    int orig_cwd_fd = -1;
    char *orig_cwd = NULL;
    char *target_dir = NULL;
    struct NFTW_STORAGE_STRUCT_NAME saved_details;

    if (!NFTW_FIND_FN_NAME(&saved_details)) {
        pseudo_diag("%s: Could not find corresponding callback!", __func__);
        return -1;
    }

    // This flag is handled by nftw, however the actual directory change happens
    // outside of pseudo, so it doesn't have any effect. To mitigate this, handle
    // it here also explicitly.
    //
    // This is very similar to what glibc is doing: keep an open FD for the
    // current working directory, process the entry (determine the flags, etc),
    // call the callback, and then switch back to the original folder - in the same
    // process. Glibc doesn't seem to take any further thread-safety measures nor
    // other special steps.
    // Error checking is not done here, as if real_nftw couldn't perform it,
    // then it has already returned an error, and this part isn't reached. This
    // just repeats the successful steps.
    //
    // See io/ftw.c in glibc source
    if (saved_details.flags & FTW_CHDIR) {
        orig_cwd_fd = open(".", O_RDONLY | O_DIRECTORY);
        if (orig_cwd_fd == -1) {
            orig_cwd = getcwd(NULL, 0);
        }

        // If it is a folder that's content has been already walked with the
        // FTW_DEPTH flag, then switch into this folder, instead of the parent of
        // it. This matches the behavior of the real nftw in this special case.
        // This seems to be undocumented - it was derived by observing this behavior.
        if (typeflag == FTW_DP) {
            chdir(fpath);
        } else {
            target_dir = malloc(ftwbuf->base + 1);
            memset(target_dir, 0, ftwbuf->base + 1);
            strncpy(target_dir, fpath, ftwbuf->base);
            chdir(target_dir);
        }
    }

    // This is the main point of this call. Instead of the stat that
    // came from real_nftw, use the stat returned by pseudo.
    // If the target can't be stat'd (DNR), then just forward whatever
    // is inside - no information can be retrieved of it anyway.
    if (typeflag != FTW_DNR) {
        (saved_details.flags & FTW_PHYS) ? NFTW_LSTAT_NAME(fpath, sb) : NFTW_STAT_NAME(fpath, sb);
    }

    int ret = saved_details.callback(fpath, sb, typeflag, ftwbuf);

    if (saved_details.flags & FTW_CHDIR) {
        if (orig_cwd_fd != -1) {
            fchdir(orig_cwd_fd);
            close(orig_cwd_fd);
        } else if (orig_cwd != NULL) {
            chdir(orig_cwd);
        }
        free(target_dir);
    }

    return ret;
}

static int
NFTW_WRAP_NAME(const char *path, int (*fn)(const char *, const struct NFTW_STAT_STRUCT *, int, struct FTW *), int nopenfd, int flag) {
    int rc = -1;

    struct NFTW_STORAGE_STRUCT_NAME saved_details;

    saved_details.tid = pthread_self();
    saved_details.flags = flag;
    saved_details.callback = fn;

    pthread_mutex_lock(&NFTW_MUTEX_NAME);
    NFTW_APPEND_FN_NAME(&saved_details);
    pthread_mutex_unlock(&NFTW_MUTEX_NAME);

#include NFTW_GUTS_INCLUDE

    pthread_mutex_lock(&NFTW_MUTEX_NAME);
    NFTW_DELETE_FN_NAME();
    pthread_mutex_unlock(&NFTW_MUTEX_NAME);
    return rc;
}
