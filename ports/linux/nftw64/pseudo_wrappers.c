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

#define CONCAT_EXPANDED(prefix, value) prefix ## value
#define CONCAT(prefix, value) CONCAT_EXPANDED(prefix, value)

#define NFTW_NAME nftw64
#define NFTW_WRAP_NAME CONCAT(wrap_, NFTW_NAME)
#define NFTW_REAL_NAME CONCAT(real_, NFTW_NAME)
#define NFTW_STAT_STRUCT stat64
#define NFTW_STAT_NAME stat64
#define NFTW_LSTAT_NAME lstat64

// this file is in the same directory as this wrapper, but
// this path is relative to the nftw_wrapper_base file, where
// it gets included.
#define NFTW_GUTS_INCLUDE "../../linux/nftw64/guts/nftw64.c"
#define NFTW_CALLBACK_NAME CONCAT(wrap_callback_, NFTW_NAME)
#define NFTW_STORAGE_STRUCT_NAME CONCAT(storage_struct_, NFTW_NAME)
#define NFTW_STORAGE_ARRAY_SIZE CONCAT(storage_size_, NFTW_NAME)
#define NFTW_MUTEX_NAME CONCAT(mutex_, NFTW_NAME)
#define NFTW_STORAGE_ARRAY_NAME CONCAT(storage_array_, NFTW_NAME)
#define NFTW_APPEND_FN_NAME CONCAT(append_to_array_, NFTW_NAME)
#define NFTW_DELETE_FN_NAME CONCAT(delete_from_array_, NFTW_NAME)
#define NFTW_FIND_FN_NAME CONCAT(find_in_array_, NFTW_NAME)

// nftw64() is identical to nftw() in all regards, except
// that it uses "stat64" structs instead of "stat", so
// the whole codebase can be reused, accounting for naming
// and type changes using the macros above.

#include "../../unix/guts/nftw_wrapper_base.c"
