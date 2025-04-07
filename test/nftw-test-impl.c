#define _GNU_SOURCE
#include <stdio.h>
#include <ftw.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#define PATH_MAX 1024
#define LAST_VAL 999
#define LAST_PATH "LAST_SENTINEL"

#define TEST_WITH_PSEUDO 1
#define TEST_WITHOUT_PSEUDO 0

#define TEST_CHDIR 1
#define TEST_NO_CHDIR 0

static int current_idx = 0;
static int* current_responses;
static char** expected_fpaths;

static int pseudo_active;
static int verify_folder = 0;
static char* base_dir = NULL;

static unsigned int expected_gid;
static unsigned int expected_uid;

static int compare_paths(const char *path1, const char *path2){
    char full_path1[PATH_MAX] = {0};
    char full_path2[PATH_MAX] = {0};

    if (path1[0] == '.'){
        strcat(full_path1, base_dir);
        strcat(full_path1, path1 + 1);
    } else {
        strcpy(full_path1, path1);
    }

    if (path2[0] == '.'){
        strcat(full_path2, base_dir);
        strcat(full_path2, path2 + 1);
    } else {
        strcpy(full_path2, path2);
    }

    return strcmp(full_path1, full_path2);
}

static int callback(const char* fpath, const struct NFTW_STAT_STRUCT *sb, int typeflag, struct FTW *ftwbuf){
    int ret = current_responses[current_idx];
//    printf("path: %s, ret: %d\n", fpath, ret);

    if (ret == LAST_VAL){
        printf("Unexpected callback, it should have stopped already! fpath: %s\n", fpath);
        return FTW_STOP;
    }

    char* expected_fpath_ending = expected_fpaths[current_idx];

    if (strcmp(expected_fpath_ending, LAST_PATH) == 0){
        printf("Unexpected fpath received: %s\n", fpath);
        return FTW_STOP;
    }

    const char* actual_fpath_ending = fpath + strlen(fpath) - strlen(expected_fpath_ending);

    if (strcmp(actual_fpath_ending, expected_fpath_ending) != 0){
        printf("Incorrect fpath received. Expected: %s, actual: %s\n", expected_fpath_ending, actual_fpath_ending);
        return FTW_STOP;
    }

    if (pseudo_active) {
        if (sb->st_gid != 0 || sb->st_uid != 0) {
            printf("Invalid uid/gid! Gid (act/exp): %d/%d, Uid (act/exp): %d/%d\n", sb->st_gid, 0, sb->st_uid, 0);
            return FTW_STOP;
        }
    } else if (sb->st_gid != expected_gid || sb->st_uid != expected_uid) {
        printf("Invalid uid/gid! Gid (act/exp): %d/%d, Uid (act/exp): %d/%d\n", sb->st_gid, expected_gid, sb->st_uid, expected_uid);
        return FTW_STOP;
    }

    if (verify_folder) {
        int res;
        char* cwd = NULL;
        cwd = getcwd(NULL, 0);

        char* exp_cwd = NULL;
        if (typeflag == FTW_DP){
            res = compare_paths(fpath, cwd);
        } else {
            char* exp_cwd = malloc(ftwbuf->base);
            memset(exp_cwd, 0, ftwbuf->base);
            strncpy(exp_cwd, fpath, ftwbuf->base - 1);
            res = compare_paths(cwd, exp_cwd);
        }

        free(cwd);
        free(exp_cwd);

        if (res != 0) {
            printf("Incorrect folder for %s\n", fpath);
            return FTW_STOP;
        }
    }

    ++current_idx;
    return ret;
}

static int run_test(int* responses, char** fpaths, int expected_retval, int with_pseudo, int flags) {
    int ret;
    current_responses = responses;
    expected_fpaths = fpaths;
    pseudo_active = with_pseudo;

    ret = NFTW_NAME("./walking", callback, 10, flags);
    current_responses = NULL;
    expected_fpaths = NULL;

    if (ret != expected_retval){
        printf("Incorrect return value. Expected: %d, actual: %d\n", expected_retval, ret);
        return 1;
    }

    if (responses[current_idx] != LAST_VAL){
        printf("Not all expected paths were walked!\n");
        return 1;
    }
    return 0;
}

static int test_skip_siblings_file_depth_walking(int with_pseudo, int change_dir){
    int responses[] = {FTW_SKIP_SIBLINGS, FTW_CONTINUE, FTW_SKIP_SIBLINGS, FTW_CONTINUE,
                       FTW_CONTINUE, FTW_CONTINUE, FTW_CONTINUE, FTW_CONTINUE, LAST_VAL};
    char* fpaths[] = {"walking/a1/b2/file5",
                      "walking/a1/b2",
                      "walking/a1/b1/c1/file",
                      "walking/a1/b1/c1",
                      "walking/a1/b1",
                      "walking/a1/b3",
                      "walking/a1",
                      "walking",
                      LAST_PATH};
    int expected_retval = 0;
    int flags = FTW_ACTIONRETVAL | FTW_DEPTH;

    // store base_dir, because the fpath returned by (n)ftw can be relative to this
    // folder - that way a full absolute path can be constructed and compared,
    // if needed.
    if (change_dir){
        flags |= FTW_CHDIR;
        base_dir = getcwd(NULL, 0);
        verify_folder = 1;
    }

    return run_test(responses, fpaths, expected_retval, with_pseudo, flags);
}

/*
 * Every time a folder entry is sent to the callback, respond with FTW_SKIP_SUBTREE.
 * This should skip that particular folder completely, and continue processing
 * with its siblings (or parent, if there are no siblings).
 * Return value is expected to be 0, default walking order.
 */
static int test_skip_subtree_on_folder(int with_pseudo){
    int responses[] = {FTW_CONTINUE, FTW_CONTINUE, FTW_SKIP_SUBTREE, FTW_SKIP_SUBTREE,
                       FTW_SKIP_SUBTREE, LAST_VAL};
    char* fpaths[] = {"walking",
                      "walking/a1",
                      "walking/a1/b2",
                      "walking/a1/b1",
                      "walking/a1/b3",
                      LAST_PATH};
    int expected_retval = 0;
    int flags = FTW_ACTIONRETVAL;

    return run_test(responses, fpaths, expected_retval, with_pseudo, flags);
}

/*
 * Arguments:
 * argv[1]: always the test name
 * argv[2], argv[3]: in case the test name refers to a test without using
 *                   pseudo (no_pseudo), then they should be the gid and uid
 *                   of the current user. Otherwise these arguments are ignored.
 *
 * skip_subtree_pseudo/skip_subtree_no_pseudo: these tests are calling nftw()
 * with the FTW_ACTIONRETVAL flag, which reacts based on the return value from the
 * callback. These tests check the call's reaction to FTW_SKIP_SUBTREE call,
 * upon which nftw() should stop processing the current folder, and continue
 * with the next sibling of the folder.
 *
 * skip_siblings_pseudo/skip_siblings_no_pseudo: very similar to skip_subtree
 * tests, but it verified FTW_SKIP_SIBLINGS response, which should stop processing
 * the current folder, and continue in its parent.
 *
 * skip_siblings_chdir_pseudo/skip_siblings_chdir_no_pseudoL same as skip_siblings
 * tests, but also pass the FTW_CHDIR flag and verify that the working directory
 * is changed as expected between callback calls.
 *
 * nftw64 call only exists on Linux in case __USE_LARGEFILE64 is defined.
 * If this is not the case, just skip this test.
 */
int main(int argc, char* argv[])
{
#if !defined(__USE_LARGEFILE64) && NFTW_NAME == nftw64
return 0
#endif
    if (argc < 2) {
        printf("Need a test name as argument\n");
        return 1;
    }

    if (argc > 2) {
        expected_gid = atoi(argv[2]);
        expected_uid = atoi(argv[3]);
    }
    
    if (strcmp(argv[1], "skip_subtree_pseudo") == 0) {
        return test_skip_subtree_on_folder(TEST_WITH_PSEUDO);
    } else if (strcmp(argv[1], "skip_subtree_no_pseudo") == 0) {
        return test_skip_subtree_on_folder(TEST_WITHOUT_PSEUDO);
    } else if (strcmp(argv[1], "skip_siblings_pseudo") == 0) {
        return test_skip_siblings_file_depth_walking(TEST_WITH_PSEUDO, TEST_NO_CHDIR);
    } else if (strcmp(argv[1], "skip_siblings_no_pseudo") == 0) {
        return test_skip_siblings_file_depth_walking(TEST_WITHOUT_PSEUDO, TEST_NO_CHDIR);
    } else if (strcmp(argv[1], "skip_siblings_chdir_pseudo") == 0) {
        return test_skip_siblings_file_depth_walking(TEST_WITH_PSEUDO, TEST_CHDIR);
    } else if (strcmp(argv[1], "skip_siblings_chdir_no_pseudo") == 0) {
        return test_skip_siblings_file_depth_walking(TEST_WITHOUT_PSEUDO, TEST_CHDIR);
    } else {
        printf("Unknown test name\n");
        return 1;
    }
}
