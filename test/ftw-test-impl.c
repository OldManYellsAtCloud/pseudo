#define _GNU_SOURCE
#include <stdio.h>
#include <ftw.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#define LAST_VAL 999
#define LAST_PATH "LAST_SENTINEL"

#define TEST_WITH_PSEUDO 1
#define TEST_WITHOUT_PSEUDO 0

static int current_idx = 0;
static int* current_responses;
static char** expected_fpaths;

static int pseudo_active;

static unsigned int expected_gid;
static unsigned int expected_uid;

static int current_recursion_level = 0;
static int max_recursion = 0;


static int callback(const char* fpath, const struct FTW_STAT_STRUCT *sb, int typeflag){
    if (current_recursion_level < max_recursion) {
        ++current_recursion_level;
        if (FTW_NAME("./walking/a1", callback, 10) != 0) {
            printf("Recursive call failed\n");
            exit(1);
        }
    }


    int ret = current_responses[current_idx];
    // printf("idx: %d, path: %s, ret: %d\n", current_idx, fpath, ret);

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

    ++current_idx;
    return ret;
}

static int run_test(int* responses, char** fpaths, int expected_retval, int with_pseudo) {
    int ret;
    current_responses = responses;
    expected_fpaths = fpaths;
    pseudo_active = with_pseudo;

    ret = FTW_NAME("./walking", callback, 10);
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

/*
 * This test just walks the whole test directory structure, and verifies that
 * all expected files are returned.
 */
static int test_walking(int with_pseudo){
    int responses[] = {FTW_CONTINUE, FTW_CONTINUE, FTW_CONTINUE, FTW_CONTINUE,
                       FTW_CONTINUE, FTW_CONTINUE, FTW_CONTINUE, FTW_CONTINUE,
                       FTW_CONTINUE, FTW_CONTINUE, FTW_CONTINUE, LAST_VAL};

    char* fpaths[] = {"walking",
                      "walking/a1",
                      "walking/a1/b2",
                      "walking/a1/b2/file5",
                      "walking/a1/b2/file4",
                      "walking/a1/b1",
                      "walking/a1/b1/c1",
                      "walking/a1/b1/c1/file",
                      "walking/a1/b1/c1/file3",
                      "walking/a1/b1/c1/file2",
                      "walking/a1/b3",
                      LAST_PATH};

    int expected_retval = 0;

    return run_test(responses, fpaths, expected_retval, with_pseudo);
}

/*
 * This test is very similar to test_walking(), but the callback at the
 * start also calls ftw(), "max_recursion" times.
 * It is trying to test pseudo's implementation of handling multiple
 * concurrent (n)ftw calls in the same thread.
 */
static int test_walking_recursion(int with_pseudo){
    max_recursion = 3;

    int responses[] = {FTW_CONTINUE, FTW_CONTINUE, FTW_CONTINUE, FTW_CONTINUE,
                       FTW_CONTINUE, FTW_CONTINUE, FTW_CONTINUE, FTW_CONTINUE,
                       FTW_CONTINUE, FTW_CONTINUE, FTW_CONTINUE, FTW_CONTINUE,
                       FTW_CONTINUE, FTW_CONTINUE, FTW_CONTINUE, FTW_CONTINUE,
                       FTW_CONTINUE, FTW_CONTINUE, FTW_CONTINUE, FTW_CONTINUE,
                       FTW_CONTINUE, FTW_CONTINUE, FTW_CONTINUE, FTW_CONTINUE,
                       FTW_CONTINUE, FTW_CONTINUE, FTW_CONTINUE, FTW_CONTINUE,
                       FTW_CONTINUE, FTW_CONTINUE, FTW_CONTINUE, FTW_CONTINUE,
                       FTW_CONTINUE, FTW_CONTINUE, FTW_CONTINUE, FTW_CONTINUE,
                       FTW_CONTINUE, FTW_CONTINUE, FTW_CONTINUE, FTW_CONTINUE,
                       FTW_CONTINUE, LAST_VAL};

    char* fpaths[] = {"walking/a1",
                      "walking/a1/b2",
                      "walking/a1/b2/file5",
                      "walking/a1/b2/file4",
                      "walking/a1/b1",
                      "walking/a1/b1/c1",
                      "walking/a1/b1/c1/file",
                      "walking/a1/b1/c1/file3",
                      "walking/a1/b1/c1/file2",
                      "walking/a1/b3",
                      "walking/a1",
                      "walking/a1/b2",
                      "walking/a1/b2/file5",
                      "walking/a1/b2/file4",
                      "walking/a1/b1",
                      "walking/a1/b1/c1",
                      "walking/a1/b1/c1/file",
                      "walking/a1/b1/c1/file3",
                      "walking/a1/b1/c1/file2",
                      "walking/a1/b3",
                      "walking/a1",
                      "walking/a1/b2",
                      "walking/a1/b2/file5",
                      "walking/a1/b2/file4",
                      "walking/a1/b1",
                      "walking/a1/b1/c1",
                      "walking/a1/b1/c1/file",
                      "walking/a1/b1/c1/file3",
                      "walking/a1/b1/c1/file2",
                      "walking/a1/b3",
                      "walking",
                      "walking/a1",
                      "walking/a1/b2",
                      "walking/a1/b2/file5",
                      "walking/a1/b2/file4",
                      "walking/a1/b1",
                      "walking/a1/b1/c1",
                      "walking/a1/b1/c1/file",
                      "walking/a1/b1/c1/file3",
                      "walking/a1/b1/c1/file2",
                      "walking/a1/b3",
                      LAST_PATH};
    int expected_retval = 0;

    return run_test(responses, fpaths, expected_retval, with_pseudo);
}

/*
 * Arguments:
 * argv[1]: always the test name
 * argv[2], argv[3]: in case the test name refers to a test without using
 *                   pseudo (no_pseudo), then they should be the gid and uid
 *                   of the current user. Otherwise these arguments are ignored.
 *
 * ftw64 call only exists on Linux in case __USE_LARGEFILE64 is defined.
 * If this is not the case, just skip this test.
 */
int main(int argc, char* argv[])
{
#if !defined(__USE_LARGEFILE64) && FTW_NAME == ftw64
return 0
#endif
    if (argc < 2) {
        printf("Need a test name as argument\n");
        return 1;
    }
    
    if (strcmp(argv[1], "pseudo_no_recursion") == 0) {
        return test_walking(TEST_WITH_PSEUDO);
    } else if (strcmp(argv[1], "no_pseudo_no_recursion") == 0) {
        expected_gid = atoi(argv[2]);
        expected_uid = atoi(argv[3]);
        return test_walking(TEST_WITHOUT_PSEUDO);
    } if (strcmp(argv[1], "pseudo_recursion") == 0) {
        return test_walking_recursion(TEST_WITH_PSEUDO);
    } if (strcmp(argv[1], "no_pseudo_recursion") == 0) {
        expected_gid = atoi(argv[2]);
        expected_uid = atoi(argv[3]);
        return test_walking_recursion(TEST_WITHOUT_PSEUDO);
    } else {
        printf("Unknown test name: %s\n", argv[1]);
        return 1;
    }
}
