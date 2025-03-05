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

#define NFTW_HANDLE_BROKEN_SYMLINK(path) \
    if (!(flag & FTW_PHYS) \
          && errno == ENOENT \
          && lstat(path, &st) == 0 \
          && S_ISLNK(st.st_mode)){ \
          \
        if ((flag & FTW_MOUNT) && orig_dev != st.st_dev){ \
            continue; \
        } \
        /* existing symlink, but points to a non-existing folder */ \
        rc = fn(path, &st, FTW_SLN, &ftw); \
    } else { \
        printf("nftw: could not access path: " \
               "%s, bailing out!\n", path); \
        rc = -1; \
    }

#define NFTW_INCREASE_INODE_LIST_IF_NEEDED \
    if (inode_list_size == inode_list_capacity) { \
        size_t new_capacity = (inode_list_capacity == 0) ? 16 : inode_list_capacity * 2; \
        ino_t *temp = realloc(inode_list, new_capacity * sizeof(ino_t)); \
        if (!temp) { \
            rc = -1; \
            errno = ENOMEM; \
            goto nftw_out; \
        } \
        inode_list = temp; \
        inode_list_capacity = new_capacity; \
    }

// this is gcc-extension
#define NFTW_WAS_INODE_SEEN_ALREADY(node, node_list, node_list_size) ({ \
    int duplicate = 0; \
    for (size_t i = 0; i < node_list_size; ++i) { \
        if (node_list[i] == node) { \
            duplicate = 1; \
            break; \
        } \
    } \
    duplicate; })

#define NFTW_CONCAT_PATH_WITH_SEPARATOR(destination, folder, separator, filename) \
    strcpy(destination, folder); \
    strcat(destination, separator); \
    strcat(destination, filename);

#define NFTW_CALCULATE_FTW_BASE_AND_LEVEL(path, ftw_struct) \
    for (size_t i = 0; path[i] != '\0'; ++i){ \
        if (path[i] == '/'){ \
            ftw_struct.level++; \
            ftw_struct.base = i + 1; \
        } \
    }


    int cwdfd = -1; // hold the original working dir's fd
    char *cwd = NULL; // if we can't get cwdfd, store working dir's name here
    struct dirent *d; // current dirent being iterated
    char** dirlist = NULL; // list of all subdirectories to be walked
    size_t dirlist_size = 0;
    size_t base_level = 0; // depth of the "path" argument. It is subtracted
                           // from the depth of the individual entries
    char folder_pathbuf[1025]; // this folder's content is being iterated
    char file_pathbuf[1025]; // this file is currently processed, within pathbuf
    struct FTW ftw; // struct to be passed to "fn"
    DIR *dir; // the currently iterated directory stream
    struct stat st; // stat to send to "fn"
    dev_t orig_dev; // the device of the top "path"

    ino_t *inode_list = NULL;
    size_t inode_list_size = 0, inode_list_capacity = 0;

    // does path make sense?
    if (path[0] == '\0'){
        errno = ENOENT;
        rc = -1;
        goto nftw_out;
    }

    // if FTW_CHDIR is set, save the current directory,
    // so at the end we can restore it
    if (flag & FTW_CHDIR){
        cwdfd = open(".", O_RDONLY | O_DIRECTORY);
        if (cwdfd == -1){
            // Uff. Try getting the directory name
            if (errno == EACCES)
                cwd = getcwd(NULL, 0);

            if (cwd == NULL){
                rc = -1;
                goto nftw_out;
            }
        }
    }

    if (flag & FTW_MOUNT){
        if (((flag & FTW_PHYS)
              ? lstat(path, &st)
              : stat(path, &st)) < 0) {
            printf("nftw: could not stat top dir: %s\n", path);
            rc = -1;
            goto nftw_out;
        } else {
            orig_dev = st.st_dev;
        }
    }

    dirlist = malloc(++dirlist_size * sizeof(char*));
    // kickstart the first element with the top folder path
    dirlist[0] = malloc(strlen(path) + 1);
    strcpy(dirlist[0], path);

    // check how many level deep the original path is
    for (size_t i = 0; path[i] != '\0'; ++i){
        if (path[i] == '/')
            ++base_level;
    }

    /*
     * Walk the tree, without FTW_DEPTH flag.
     * 1. Take the last folder from the list
     * 2. Stat it, call fn
     * 3. Collect all files from the folder, stat them, and call fn
     * 4. Collect all folders from the folder, and add them to the list
     * 5. Repeat until the list if empty
     */
    while (dirlist_size > 0){
        ftw.level = -base_level;
        ftw.base = 0;

        // 1. take the last folder from the list
        strcpy(folder_pathbuf, dirlist[dirlist_size - 1]);
        free(dirlist[dirlist_size - 1]);
        dirlist = realloc(dirlist, --dirlist_size * sizeof(char*));

        NFTW_CALCULATE_FTW_BASE_AND_LEVEL(folder_pathbuf, ftw);

        if (flag & FTW_CHDIR){
            chdir(folder_pathbuf);
            chdir("..");
        }

        // 2. stat it, call fn -- first only the folder, not its content
        if (((flag & FTW_PHYS)
              ? lstat(folder_pathbuf, &st)
              : stat(folder_pathbuf, &st)) < 0) {
            NFTW_HANDLE_BROKEN_SYMLINK(folder_pathbuf);
        } else {
            if ((flag & FTW_MOUNT) && orig_dev != st.st_dev)
                continue;

            int is_duplicate = NFTW_WAS_INODE_SEEN_ALREADY(st.st_ino, inode_list, inode_list_size);
            if (is_duplicate)
                continue;
            //NFTW_CHECK_AND_CONTINUE_IF_INODE_WAS_SEEN_ALREADY;
            NFTW_INCREASE_INODE_LIST_IF_NEEDED;
            inode_list[inode_list_size++] = st.st_ino;

            // Usually this can be only a folder or symlink, except if the whole
            // function was called with a filename as path, instead of a folder path.
            // In that case it is a file, and this is the first and only iteration.
            int ent_flag = S_ISDIR(st.st_mode) ? FTW_D :
                           S_ISLNK(st.st_mode) ? FTW_SL : FTW_F;
            rc = fn(folder_pathbuf, &st, ent_flag, &ftw);
        }

        if (rc < 0)
            goto nftw_out;

        dir = opendir(folder_pathbuf);
        if (!dir)
            continue;

        // 3. Collect all files from the folder, stat them, and call fn
        while ((d = readdir(dir)) != NULL){
            // not interested in technical folders
            if ((strcmp(d->d_name, ".") == 0) || strcmp(d->d_name, "..") == 0){
                continue;
            }

            ftw.level = -base_level;
            ftw.base = 0;

            NFTW_CONCAT_PATH_WITH_SEPARATOR(file_pathbuf, folder_pathbuf, "/", d->d_name);
            NFTW_CALCULATE_FTW_BASE_AND_LEVEL(file_pathbuf, ftw);

            // 4. Collect all folders from the folder, and add them to the list
            if (d->d_type == DT_DIR) {
                dirlist = realloc(dirlist, ++dirlist_size * sizeof(char*));
                dirlist[dirlist_size - 1] = malloc(strlen(file_pathbuf) + 1);
                strcpy(dirlist[dirlist_size - 1], file_pathbuf);
                continue;
            }

            if (flag & FTW_CHDIR){
                chdir(folder_pathbuf);
            }

            // get stat from pseudo
            if (((flag & FTW_PHYS)
                  ? lstat(file_pathbuf, &st)
                  : stat(file_pathbuf, &st)) < 0) {

                NFTW_HANDLE_BROKEN_SYMLINK(file_pathbuf);
            } else {
                if ((flag & FTW_MOUNT) && orig_dev != st.st_dev)
                    continue;

                int is_duplicate = NFTW_WAS_INODE_SEEN_ALREADY(st.st_ino, inode_list, inode_list_size);
                if (is_duplicate)
                    continue;

                NFTW_INCREASE_INODE_LIST_IF_NEEDED;
                inode_list[inode_list_size++] = st.st_ino;

                int ent_flag = S_ISDIR(st.st_mode) ? FTW_D :
                               S_ISLNK(st.st_mode) ? FTW_SL : FTW_F;
                rc = fn(file_pathbuf, &st, ent_flag, &ftw);
            }

            if (rc < 0)
                goto nftw_out;
        }
    }

nftw_out:
    // get back to the original folder, if needed
    if (cwdfd != -1){
        int save_errno = errno;
        fchdir(cwdfd);
        close(cwdfd);
        errno = save_errno;
    } else if (cwd != NULL){
        int save_errno = errno;
        chdir(cwd);
        free(cwd);
        errno = save_errno;
    }

    for (size_t i = 0; i < dirlist_size; ++i)
        free(dirlist[i]);

    free(dirlist);
    free(inode_list);

/*	return rc;
 * }
 */
