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

    int cwdfd = -1; // hold the original working dir's fd
    char *cwd = NULL; // if we can't get cwdfd, store working dir's name here
    struct dirent *d; // current dirent being iterated
    char** dirlist = NULL; // list of all subdirectories to be walked
    size_t dirlist_size = 0;
    size_t dirlist_idx = 0;
    size_t base_level = 0; // depth of the "path" argument. It is subtracted
                           // from the depth of the individual entries
    char pathbuf[1025]; // this path's content is being iterated
    char cur_pathbuf[1025]; // this path is currently processed, within pathbuf
    struct FTW ftw; // struct to be passed to "fn"
    DIR *dir; // the currently iterated directory stream
    struct stat st; // stat to send to "fn"

    ino_t *inode_list = NULL;
    size_t inode_list_size = 0, inode_list_capacity = 0;

    // does path make sense?
    if (path[0] == '\0'){
        errno = ENOENT;
        rc = -1;
        goto nftw_out_fail;
    }

    if (flag & FTW_MOUNT || flag & FTW_DEPTH || flag & FTW_ACTIONRETVAL)
        pseudo_diag("Warning: nftw was called with unimplemented flags. \
                     Please report it with the recipe name that triggered this.\n");

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
                goto nftw_out_fail;
            }
        }
    }

    // check how many level deep the original path is
    for (size_t i = 0; path[i] != '\0'; ++i){
        if (path[i] == '/')
            ++base_level;
    }

    /*
     * Walk the tree. Currently only breadth-first.
     * Start with the path from the arguments, and afterwards keep getting
     * the oldest one from the dirlist array. Open each items, and call
     * fn and stat on them. In case the entity is a folder, then add it
     * to the dirlist, so it can be iterated also.
     */
    do {
        if (dirlist_size == 0) {
            strcpy(pathbuf, path);
        } else {
            strcpy(pathbuf, dirlist[dirlist_idx++]);
        }

        dir = opendir(pathbuf);
        if (!dir) continue;

        while ((d = readdir(dir)) != NULL) {

            // not interested in these technical entries
            // unless it is the starting path(".", with empty dirlist), which however needs to be done
            if ((strcmp(d->d_name, ".") == 0 && dirlist_size > 0)
                || strcmp(d->d_name, "..") == 0){
                continue;
            }

            ftw.level = -base_level;
            ftw.base = 0;

            strcpy(cur_pathbuf, pathbuf);
            strcat(cur_pathbuf, "/");
            strcat(cur_pathbuf, d->d_name);

            pseudo_debug(PDBGF_VERBOSE, "nftw: processing path: %s\n", pathbuf);

            // set the values in ftw struct, which is passed to fn
            // level: how many folders deep the current file is
            // base: starting index of the base filename (in practice,
            // the index after the last folder separator)
            for (size_t i = 0; cur_pathbuf[i] != '\0'; ++i){
                if (cur_pathbuf[i] == '/'){
                    ftw.level++;
                    ftw.base = i + 1;
                }
            }

            // if it's the top-dir, just force the level to 0
            if (dirlist_size == 0)
                ftw.level = 0;


            if (d->d_type == DT_DIR) {
                dirlist = realloc(dirlist, ++dirlist_size * sizeof(char*));
                dirlist[dirlist_size - 1] = malloc(sizeof(cur_pathbuf));
                strcpy(dirlist[dirlist_size - 1], cur_pathbuf);
            }

            if (flag & FTW_CHDIR){
                chdir(pathbuf);
            }

            // get stat from pseudo
            if (((flag & FTW_PHYS)
                  ? lstat(cur_pathbuf, &st)
                  : stat(cur_pathbuf, &st)) < 0) {

                if (!(flag & FTW_PHYS)
                      && errno == ENOENT
                      && lstat(cur_pathbuf, &st) == 0
                      && S_ISLNK(st.st_mode)){
                    // existing symlink, but points to a non-existing file
                    rc = fn(cur_pathbuf, &st, FTW_SLN, &ftw);
                } else {
                    pseudo_debug(PDBGF_VERBOSE, "nftw: could not access path: "
                                                "%s, bailing out!\n", pathbuf);
                    rc = -1;
                }
            } else {
                int duplicate = 0;
                for (size_t i = 0; i < inode_list_size; ++i) {
                    if (inode_list[i] == st.st_ino) {
                        duplicate = 1;
                        break;
                    }
                }
                if (duplicate) continue;

                if (inode_list_size == inode_list_capacity) {
                    size_t new_capacity = (inode_list_capacity == 0) ? 16 : inode_list_capacity * 2;
                    ino_t *temp = realloc(inode_list, new_capacity * sizeof(ino_t));
                    if (!temp) {
                        rc = -1;
                        goto nftw_out_fail;
                    }
                    inode_list = temp;
                    inode_list_capacity = new_capacity;
                }
                inode_list[inode_list_size++] = st.st_ino;

                int ent_flag = S_ISDIR(st.st_mode) ? FTW_D :
                               S_ISLNK(st.st_mode) ? FTW_SL : FTW_F;
                rc = fn(cur_pathbuf, &st, ent_flag, &ftw);
            }

            // if there was a failure at any point, then it
            // is finished, return error.
            if (rc < 0) {
                pseudo_debug(PDBGF_VERBOSE, "nftw: callback function returned error\n");
                break;
            }
        }
        closedir(dir);
    } while (dirlist_idx < dirlist_size);

nftw_out_fail:
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
