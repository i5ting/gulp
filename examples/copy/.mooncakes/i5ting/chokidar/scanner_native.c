#include "moonbit.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char *chokidar_scanner_cstring_from_bytes(moonbit_bytes_t bytes) {
  size_t len = (size_t)Moonbit_array_length(bytes);
  char *text = (char *)malloc(len + 1);
  if (text == NULL) {
    return NULL;
  }
  if (len > 0) {
    memcpy(text, bytes, len);
  }
  text[len] = '\0';
  return text;
}

static moonbit_bytes_t chokidar_scanner_bytes_from_cstr(const char *text) {
  if (text == NULL) text = "";
  size_t len = strlen(text);
  moonbit_bytes_t bytes = moonbit_make_bytes((int32_t)len, 0);
  if (len > 0) {
    memcpy(bytes, text, len);
  }
  return bytes;
}

/* Single-call stat cache: avoids redundant stat() when size/mode/mtime are
   queried together for the same path. */
static struct stat chokidar_stat_cache;
static char chokidar_stat_cache_path[4096];
static int chokidar_stat_cache_follow;
static int chokidar_stat_cache_valid = 0;

static int chokidar_do_stat(const char *path, int follow_symlink) {
  if (chokidar_stat_cache_valid &&
      chokidar_stat_cache_follow == follow_symlink &&
      strcmp(chokidar_stat_cache_path, path) == 0) {
    return 0;
  }
  int status = follow_symlink ? stat(path, &chokidar_stat_cache)
                              : lstat(path, &chokidar_stat_cache);
  if (status != 0) {
    chokidar_stat_cache_valid = 0;
    return -1;
  }
  strncpy(chokidar_stat_cache_path, path,
          sizeof(chokidar_stat_cache_path) - 1);
  chokidar_stat_cache_path[sizeof(chokidar_stat_cache_path) - 1] = '\0';
  chokidar_stat_cache_follow = follow_symlink;
  chokidar_stat_cache_valid = 1;
  return 0;
}

MOONBIT_FFI_EXPORT int64_t
chokidar_scanner_modified_nanos(moonbit_bytes_t path, int follow_symlink) {
  char *path_text = chokidar_scanner_cstring_from_bytes(path);
  if (path_text == NULL) {
    return 0;
  }
  int status = chokidar_do_stat(path_text, follow_symlink);
  free(path_text);
  if (status != 0) {
    return 0;
  }
  int64_t result;
#if defined(__APPLE__)
  result = ((int64_t)chokidar_stat_cache.st_mtimespec.tv_sec * 1000000000LL) +
           (int64_t)chokidar_stat_cache.st_mtimespec.tv_nsec;
#else
  result = ((int64_t)chokidar_stat_cache.st_mtim.tv_sec * 1000000000LL) +
           (int64_t)chokidar_stat_cache.st_mtim.tv_nsec;
#endif
  /* Invalidate after use so a later scan of the same path gets a fresh stat. */
  chokidar_stat_cache_valid = 0;
  return result;
}

MOONBIT_FFI_EXPORT int
chokidar_scanner_stat_valid(moonbit_bytes_t path, int follow_symlink) {
  char *path_text = chokidar_scanner_cstring_from_bytes(path);
  if (path_text == NULL) return 0;
  int ok = chokidar_do_stat(path_text, follow_symlink) == 0 ? 1 : 0;
  free(path_text);
  return ok;
}

MOONBIT_FFI_EXPORT int64_t
chokidar_scanner_stat_size(moonbit_bytes_t path, int follow_symlink) {
  char *path_text = chokidar_scanner_cstring_from_bytes(path);
  if (path_text == NULL) return 0;
  if (chokidar_do_stat(path_text, follow_symlink) != 0) {
    free(path_text);
    return 0;
  }
  free(path_text);
  return (int64_t)chokidar_stat_cache.st_size;
}

MOONBIT_FFI_EXPORT int
chokidar_scanner_stat_mode(moonbit_bytes_t path, int follow_symlink) {
  char *path_text = chokidar_scanner_cstring_from_bytes(path);
  if (path_text == NULL) return 0;
  if (chokidar_do_stat(path_text, follow_symlink) != 0) {
    free(path_text);
    return 0;
  }
  free(path_text);
  return (int)chokidar_stat_cache.st_mode;
}

MOONBIT_FFI_EXPORT int
chokidar_test_chmod(moonbit_bytes_t path, int mode) {
  char *path_text = chokidar_scanner_cstring_from_bytes(path);
  if (path_text == NULL) return -1;
  int result = chmod(path_text, (mode_t)mode);
  free(path_text);
  return result;
}

MOONBIT_FFI_EXPORT int
chokidar_test_setenv(moonbit_bytes_t name, moonbit_bytes_t value) {
  char *name_text = chokidar_scanner_cstring_from_bytes(name);
  char *value_text = chokidar_scanner_cstring_from_bytes(value);
  if (name_text == NULL || value_text == NULL) {
    free(name_text);
    free(value_text);
    return -1;
  }
  int result = setenv(name_text, value_text, 1);
  free(name_text);
  free(value_text);
  return result;
}

MOONBIT_FFI_EXPORT int
chokidar_test_unsetenv(moonbit_bytes_t name) {
  char *name_text = chokidar_scanner_cstring_from_bytes(name);
  if (name_text == NULL) return -1;
  int result = unsetenv(name_text);
  free(name_text);
  return result;
}

MOONBIT_FFI_EXPORT moonbit_bytes_t
chokidar_scanner_realpath(moonbit_bytes_t path) {
  char *path_text = chokidar_scanner_cstring_from_bytes(path);
  if (path_text == NULL) return chokidar_scanner_bytes_from_cstr("");
  char resolved[PATH_MAX];
  char *result = realpath(path_text, resolved);
  free(path_text);
  if (result == NULL) return chokidar_scanner_bytes_from_cstr("");
  return chokidar_scanner_bytes_from_cstr(result);
}
