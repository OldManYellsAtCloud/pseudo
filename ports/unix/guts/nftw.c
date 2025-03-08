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

// returning a value from macro is gcc-extension
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

    // used with FTW_DEPTH flag, where multiple dirstreams are kept open,
    // and keeping track of their path is important, and it needs to be
    // opened or concatenated with filenames at various points.
    struct dirstream_path {
        DIR* dirstream;
        char* path;
    };


    int cwdfd = -1; // hold the original working dir's fd
    char *cwd = NULL; // if we can't get cwdfd, store working dir's name here
    struct dirent *d; // current dirent being iterated
    char** dirlist = NULL; // list of all subdirectories to be walked,
                           // without FTW_DEPTH flag
    size_t dirlist_size = 0;
    size_t base_level = 0; // depth of the "path" argument. It is subtracted
                           // from the depth of the individual entries
    char folder_pathbuf[1025]; // this folder's content is being iterated
    char file_pathbuf[1025]; // this file is currently processed, within pathbuf
    struct FTW ftw; // struct to be passed to "fn"
    DIR *dir; // the currently iterated directory stream
    struct stat st; // stat to send to "fn"
    dev_t orig_dev; // the device of the top "path"

    struct dirstream_path* dirstreamlist = NULL; // list of currently processed folders,
                                                 // used along with FTW_DEPTH flag
    size_t dirstreamlist_size = 0;

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

    // check how many level deep the original path is
    for (size_t i = 0; path[i] != '\0'; ++i){
        if (path[i] == '/')
            ++base_level;
    }

    if (!(flag & FTW_DEPTH))
        goto nftw_no_ftw_depth;

    // check if it's a folder at all
    stat(path, &st);

    if (S_ISDIR(st.st_mode)){
        dirstreamlist = malloc(++dirstreamlist_size * sizeof(struct dirstream_path));
        dirstreamlist[0].path = malloc(strlen(path) + 1);
        dirstreamlist[0].dirstream = malloc(sizeof(DIR*));
        strcpy(dirstreamlist[0].path, path);
        dirstreamlist[0].dirstream = opendir(path);
    } else {
        // otherwise it's a single file, just send it off
        ftw.level = -base_level;
        ftw.base = 0;
        NFTW_CALCULATE_FTW_BASE_AND_LEVEL(path, ftw);
        if (flag & FTW_CHDIR){
            char *parent = malloc(ftw.base + 1);
            strncpy(parent, path, ftw.base);
            chdir(parent);
            free(parent);
        }

        rc = fn(path, &st, S_ISLNK(st.st_mode) ? FTW_SL : FTW_F, &ftw);
    }



    /*
     * Walking with FTW_DEPTH flag.
     * The idea is the following:
     * 1.  Get the top dirstream from the dirstreamlist stack
     * 2.  Read the next entry from it.
     * 3a. If the entry is a file, stat it, and send it to fn.
     * 3b. If the entry is a folder, open a directory stream with it,
     *     and push it on top of the stack.
     * 3c. If the entry is NULL, then stat the dirstream's folder itself,
     *     send it to fn, and pop it from the stack.
     * 4.  Repeat until the stack is empty.
     */
    while (dirstreamlist_size > 0){
        if ((d = readdir(dirstreamlist[dirstreamlist_size - 1].dirstream)) != NULL){
            if ((strcmp(d->d_name, ".") == 0) || strcmp(d->d_name, "..") == 0){
                continue;
            }

            NFTW_CONCAT_PATH_WITH_SEPARATOR(file_pathbuf, dirstreamlist[dirstreamlist_size - 1].path, "/", d->d_name);

            if (d->d_type == DT_DIR){
                dirstreamlist = realloc(dirstreamlist, ++dirstreamlist_size * sizeof(struct dirstream_path));
                dirstreamlist[dirstreamlist_size - 1].path = malloc(strlen(file_pathbuf) + 1);
                dirstreamlist[dirstreamlist_size - 1].dirstream = malloc(sizeof(DIR*));
                strcpy(dirstreamlist[dirstreamlist_size - 1].path, file_pathbuf);
                dirstreamlist[dirstreamlist_size - 1].dirstream = opendir(file_pathbuf);
                continue;
            }

            if (flag & FTW_CHDIR){
                chdir(folder_pathbuf);
            }

            ftw.level = -base_level;
            ftw.base = 0;

            NFTW_CALCULATE_FTW_BASE_AND_LEVEL(file_pathbuf, ftw);

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

                // This can't be a directory, otherwise it would have hit the
                // "continue" a few lines back.
                int ent_flag = S_ISLNK(st.st_mode) ? FTW_SL : FTW_F;
                rc = fn(file_pathbuf, &st, ent_flag, &ftw);
            }
        } else {
            // all content of the folder has been processed. it can be
            // sent to fn, and removed from the list

            ftw.level = -base_level;
            ftw.base = 0;
            NFTW_CALCULATE_FTW_BASE_AND_LEVEL(dirstreamlist[dirstreamlist_size - 1].path, ftw);

            if (flag & FTW_CHDIR){
                chdir(dirstreamlist[dirstreamlist_size - 1].path);
                chdir("..");
            }

            if (((flag & FTW_PHYS)
                  ? lstat(dirstreamlist[dirstreamlist_size - 1].path, &st)
                  : stat(dirstreamlist[dirstreamlist_size - 1].path, &st)) < 0) {
                NFTW_HANDLE_BROKEN_SYMLINK(dirstreamlist[dirstreamlist_size - 1].path);
            } else {
                if ((flag & FTW_MOUNT) && orig_dev != st.st_dev)
                    continue;

                int is_duplicate = NFTW_WAS_INODE_SEEN_ALREADY(st.st_ino, inode_list, inode_list_size);
                if (is_duplicate)
                    continue;

                NFTW_INCREASE_INODE_LIST_IF_NEEDED;
                inode_list[inode_list_size++] = st.st_ino;

                // Usually this can be only a folder or symlink, except if the whole
                // function was called with a filename as path, instead of a folder path.
                // In that case it is a file, and this is the first and only iteration.
                int ent_flag = S_ISDIR(st.st_mode) ? FTW_DP :
                               S_ISLNK(st.st_mode) ? FTW_SL : FTW_F;
                rc = fn(dirstreamlist[dirstreamlist_size - 1].path, &st, ent_flag, &ftw);
            }

            closedir(dirstreamlist[dirstreamlist_size - 1].dirstream);
            free(dirstreamlist[dirstreamlist_size - 1].path);
            dirstreamlist = realloc(dirstreamlist, --dirstreamlist_size * sizeof(struct dirstream_path));
        }

        if (flag & FTW_ACTIONRETVAL){
            if (rc == FTW_SKIP_SIBLINGS){
                // fast forward to the end of the current folder, without processing
                while (readdir(dirstreamlist[dirstreamlist_size - 1].dirstream) != NULL);
            } else if (rc == FTW_STOP) {
                goto nftw_out;
            }
            // FTW_SKIP_SUBTREE doesn't seem to make any sense when FTW_DEPTH
            // is in use, so it is not handled. It supposed to skip processing the
            // folder that is passed to fn, but when it happens with FTW_DEPTH,
            // processing has already finished...
        }

        if (!(flag & FTW_ACTIONRETVAL) && rc != 0)
            goto nftw_out;
    }

    goto nftw_out;

nftw_no_ftw_depth:
    dirlist = malloc(++dirlist_size * sizeof(char*));
    // kickstart the first element with the top folder path
    dirlist[0] = malloc(strlen(path) + 1);
    strcpy(dirlist[0], path);

    /*
     * Walk the tree, without FTW_DEPTH flag.
     * 1. Take the last folder from the list
     * 2. Stat it, call fn
     * 3. Collect all files from the folder, stat them, and call fn
     * 4. Collect all folders from the folder, and add them to the end of the list
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

            NFTW_INCREASE_INODE_LIST_IF_NEEDED;
            inode_list[inode_list_size++] = st.st_ino;

            // Usually this can be only a folder or symlink, except if the whole
            // function was called with a filename as path, instead of a folder path.
            // In that case it is a file, and this is the first and only iteration.
            int ent_flag = S_ISDIR(st.st_mode) ? FTW_D :
                           S_ISLNK(st.st_mode) ? FTW_SL : FTW_F;
            rc = fn(folder_pathbuf, &st, ent_flag, &ftw);
        }

        if (flag & FTW_ACTIONRETVAL){
            if (rc == FTW_STOP){
                goto nftw_out;
            } else if (rc == FTW_SKIP_SUBTREE){
                // jump to the next entry instead of doing anything in this folder
                continue;
            } else if (rc == FTW_SKIP_SIBLINGS){
                // this looks lik the most complex case. This is a folder,
                // so we want to go in the parent of this folder, and skip
                // everything that's inside.
                // Remove all entries from dirlist that starts with the parent
                // of this folder, and continue from there.

                // it could be smaller, but for brevity keep it this. It is
                // big enough for sure.
                if (strcmp("/", folder_pathbuf) == 0)
                    continue;

                char* parent_folder = malloc(ftw.base + 1);
                memcpy(parent_folder, folder_pathbuf, ftw.base - 1);
                parent_folder[ftw.base] = '\0';

                // But ... uhhh... TODO: that's not very great.
                // Find all the sibling folders, and remove them from the list.
                for (size_t i = 0; i < dirlist_size; ++i){
                    if ((strncmp(parent_folder, dirlist[i], strlen(parent_folder)) == 0)){
                        for (size_t j = i + 1; j < dirlist_size; ++j){
                            dirlist[j - 1] = dirlist[j];
                        }
                        dirlist = realloc(dirlist, --dirlist_size * sizeof(char*));
                    }
                }

                continue;

            }
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

            NFTW_CONCAT_PATH_WITH_SEPARATOR(file_pathbuf, folder_pathbuf, "/", d->d_name);

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

            ftw.level = -base_level;
            ftw.base = 0;

            NFTW_CALCULATE_FTW_BASE_AND_LEVEL(file_pathbuf, ftw);

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

                // This can't be a directory, otherwise it would have hit the
                // "continue" a few lines back.
                int ent_flag = S_ISLNK(st.st_mode) ? FTW_SL : FTW_F;
                rc = fn(file_pathbuf, &st, ent_flag, &ftw);
            }

            if (flag & FTW_ACTIONRETVAL){
                if (rc == FTW_STOP) {
                    goto nftw_out;
                } else if (rc == FTW_SKIP_SIBLINGS){
                    // fast forward to the end of the current folder
                    while (readdir(dir) != NULL);
                }
                // this 2nd branch doesn't handle folders, so FTW_SKIP_SUBTREE
                // needs to handling (it was handled after the folder handling)
            }


            if (!(flag & FTW_ACTIONRETVAL) && rc != 0)
                goto nftw_out;
        }
        closedir(dir);
    }

nftw_out: ;
    // get back to the original folder, if needed
    int save_errno = errno;
    if (cwdfd != -1){
        fchdir(cwdfd);
        close(cwdfd);
    } else if (cwd != NULL){
        chdir(cwd);
        free(cwd);
    }

    for (size_t i = 0; i < dirlist_size; ++i)
        free(dirlist[i]);

    free(dirlist);

    for (size_t i = 0; i < dirstreamlist_size; ++i){
        closedir(dirstreamlist[i].dirstream);
        free(dirstreamlist[i].path);
    }

    free(dirstreamlist);
    free(inode_list);
    errno = save_errno;


/*	return rc;
 * }
 */
