#define _GNU_SOURCE
#include <stdio.h>
#include <ftw.h>
#include <string.h>

#define LAST_VAL  "LAST_SENTINEL"

char *default_walk_order_results[] = {"walking",
                                     "walking/a1",
                                     "walking/a1/b1",
                                     "walking/a1/b1/c1",
                                     "walking/a1/b1/c1/file",
                                     LAST_VAL};

char *depth_walk_order_results[] = {"walking/a1/b1/c1/file",
                                    "walking/a1/b1/c1",
                                    "walking/a1/b1",
                                    "walking/a1",
                                    "walking",
                                    LAST_VAL};

char *single_file_walk_results[] = {"walking/a1/b1/c1/file",
                                   LAST_VAL};

static int expected_result_idx;
char** current_expected_test_results;

static int callback(const char* fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf){
    const char *expected_fpath_ending = current_expected_test_results[expected_result_idx++];
    const char *actual_fpath_ending = fpath + strlen(fpath) - strlen(expected_fpath_ending);

    if (strcmp(expected_fpath_ending, LAST_VAL) == 0){
        printf("Unexpected file entry received: %s\n", fpath);
        return FTW_STOP;
    }

    if (strlen(expected_fpath_ending) != strlen(actual_fpath_ending)){
        printf("Incorrect path length! Expected: %s, Actual: %s\n", expected_fpath_ending, actual_fpath_ending);
        return FTW_STOP;
    }

    if (strcmp(expected_fpath_ending, actual_fpath_ending) != 0){
        printf("Incorrect path ending! Expected: %s, actual: %s\n", expected_fpath_ending, actual_fpath_ending);
        return FTW_STOP;
    }

    return 0;
}

static int test_default_walk_order(){
    expected_result_idx = 0;
    current_expected_test_results = default_walk_order_results;

    return nftw("./walking", callback, 10, 0);
}

static int test_depth_walk_order(){
    expected_result_idx = 0;
    current_expected_test_results = depth_walk_order_results;

    return nftw("./walking", callback, 10, FTW_DEPTH);
}

static int test_single_file_walking(){
    expected_result_idx = 0;
    current_expected_test_results = single_file_walk_results;

    return nftw("./walking/a1/b1/c1/file", callback, 10, 0);
}

static int test_single_file_walking_depth(){
    expected_result_idx = 0;
    current_expected_test_results = single_file_walk_results;

    return nftw("./walking/a1/b1/c1/file", callback, 10, FTW_DEPTH);
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        printf("Need a test name as argument\n");
        return 1;
    }

    if (strcmp(argv[1], "depth_folder") == 0) {
        printf("depth folder\n");
        return test_depth_walk_order();
    } else if (strcmp(argv[1], "normal_folder") == 0) {
        printf("nornal folder\n");
        return test_default_walk_order();
    } else if (strcmp(argv[1], "depth_file") == 0) {
        printf("file depth\n");
        return test_single_file_walking_depth();
    } else if (strcmp(argv[1], "normal_file") == 0) {
        printf("file normal\n");
        return test_single_file_walking();
    } else {
        perror("Invalid test name\n");
        return 1;
    }
}
