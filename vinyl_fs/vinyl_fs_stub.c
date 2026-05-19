#include <moonbit.h>

#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32) && !defined(_WIN64)
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <utime.h>
#include <unistd.h>
#else
#include <direct.h>
#include <sys/stat.h>
#include <sys/utime.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
#define MULP_VINYL_FS_MKDIR(path) _mkdir(path)
#else
#define MULP_VINYL_FS_MKDIR(path) mkdir(path, 0777)
#endif

typedef struct {
  char *data;
  size_t length;
  size_t capacity;
} MulpVinylFsBuffer;

typedef struct {
  FILE *file;
  int32_t skipped;
} MulpVinylFsDestWriter;

static int mulp_vinyl_fs_ensure_parent_dirs(const char *path);
static int mulp_vinyl_fs_mkdir_if_needed(char *path);
static void mulp_vinyl_fs_dest_writer_finalize(void *ptr);

static int mulp_vinyl_fs_buffer_append(MulpVinylFsBuffer *buffer,
                                        const char *text) {
  size_t length = strlen(text);
  if (buffer->length + length + 1 > buffer->capacity) {
    size_t next_capacity = buffer->capacity == 0 ? 256 : buffer->capacity;
    while (buffer->length + length + 1 > next_capacity) {
      next_capacity *= 2;
    }
    char *next = (char *)realloc(buffer->data, next_capacity);
    if (next == NULL) {
      return 0;
    }
    buffer->data = next;
    buffer->capacity = next_capacity;
  }
  memcpy(buffer->data + buffer->length, text, length);
  buffer->length += length;
  buffer->data[buffer->length] = '\0';
  return 1;
}

static moonbit_bytes_t mulp_vinyl_fs_buffer_to_bytes(
    MulpVinylFsBuffer *buffer) {
  moonbit_bytes_t result = moonbit_make_bytes((int32_t)buffer->length, 0);
  if (buffer->length > 0) {
    memcpy(result, buffer->data, buffer->length);
  }
  free(buffer->data);
  return result;
}

#if !defined(_WIN32) && !defined(_WIN64)
static int mulp_vinyl_fs_join(char *out, size_t out_size, const char *left,
                              const char *right) {
  size_t left_length = strlen(left);
  const char *separator = left_length > 0 && left[left_length - 1] == '/'
                              ? ""
                              : "/";
  return snprintf(out, out_size, "%s%s%s", left, separator, right) <
         (int)out_size;
}

static int mulp_vinyl_fs_walk(const char *root, const char *rel,
                              MulpVinylFsBuffer *buffer) {
  char dir_path[PATH_MAX];
  if (rel[0] == '\0') {
    if (strlen(root) >= sizeof(dir_path)) {
      return 0;
    }
    strcpy(dir_path, root);
  } else if (!mulp_vinyl_fs_join(dir_path, sizeof(dir_path), root, rel)) {
    return 0;
  }

  DIR *dir = opendir(dir_path);
  if (dir == NULL) {
    return 1;
  }

  struct dirent *entry = NULL;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    char child_rel[PATH_MAX];
    if (rel[0] == '\0') {
      if (strlen(entry->d_name) >= sizeof(child_rel)) {
        closedir(dir);
        return 0;
      }
      strcpy(child_rel, entry->d_name);
    } else if (!mulp_vinyl_fs_join(child_rel, sizeof(child_rel), rel,
                                   entry->d_name)) {
      closedir(dir);
      return 0;
    }

    char child_abs[PATH_MAX];
    if (!mulp_vinyl_fs_join(child_abs, sizeof(child_abs), root, child_rel)) {
      closedir(dir);
      return 0;
    }

    if (!mulp_vinyl_fs_buffer_append(buffer, child_rel) ||
        !mulp_vinyl_fs_buffer_append(buffer, "\n")) {
      closedir(dir);
      return 0;
    }

    struct stat info;
    if (lstat(child_abs, &info) == 0 && S_ISDIR(info.st_mode) &&
        !S_ISLNK(info.st_mode)) {
      if (!mulp_vinyl_fs_walk(root, child_rel, buffer)) {
        closedir(dir);
        return 0;
      }
    }
  }

  closedir(dir);
  return 1;
}

static moonbit_bytes_t mulp_vinyl_fs_metadata_bytes(const char *path,
                                                    int follow_symlink) {
  struct stat info;
  int stat_result = follow_symlink ? stat(path, &info) : lstat(path, &info);
  if (stat_result != 0) {
    return moonbit_make_bytes(0, 0);
  }
  const char *kind = "other";
  if (S_ISREG(info.st_mode)) {
    kind = "file";
  } else if (S_ISDIR(info.st_mode)) {
    kind = "directory";
  } else if (S_ISLNK(info.st_mode)) {
    kind = "symlink";
  }
  char buffer[160];
  int length = snprintf(buffer, sizeof(buffer), "%s\n%lld\n%lld\n%o\n%lld\n", kind,
                        (long long)info.st_size,
                        (long long)info.st_mtime * 1000LL,
                        (unsigned int)(info.st_mode & 07777),
                        (long long)info.st_ctime * 1000LL);
  moonbit_bytes_t result = moonbit_make_bytes(length, 0);
  memcpy(result, buffer, (size_t)length);
  return result;
}
#endif

MOONBIT_FFI_EXPORT
moonbit_bytes_t mulp_vinyl_fs_discover_paths(moonbit_bytes_t root) {
  MulpVinylFsBuffer buffer = {0};
#if defined(_WIN32) || defined(_WIN64)
  (void)root;
#else
  (void)mulp_vinyl_fs_walk((const char *)root, "", &buffer);
#endif
  return mulp_vinyl_fs_buffer_to_bytes(&buffer);
}

MOONBIT_FFI_EXPORT
moonbit_bytes_t mulp_vinyl_fs_read_symlink(moonbit_bytes_t path) {
#if defined(_WIN32) || defined(_WIN64)
  (void)path;
  return moonbit_make_bytes(0, 0);
#else
  char target[PATH_MAX];
  ssize_t length = readlink((const char *)path, target, sizeof(target) - 1);
  if (length <= 0) {
    return moonbit_make_bytes(0, 0);
  }
  moonbit_bytes_t result = moonbit_make_bytes((int32_t)length, 0);
  memcpy(result, target, (size_t)length);
  return result;
#endif
}

MOONBIT_FFI_EXPORT
moonbit_bytes_t mulp_vinyl_fs_stat_metadata(moonbit_bytes_t path) {
#if defined(_WIN32) || defined(_WIN64)
  struct stat info;
  if (stat((const char *)path, &info) != 0) {
    return moonbit_make_bytes(0, 0);
  }
  const char *kind = "other";
  if (S_ISREG(info.st_mode)) {
    kind = "file";
  } else if (S_ISDIR(info.st_mode)) {
    kind = "directory";
  }
  char buffer[160];
  int length = snprintf(buffer, sizeof(buffer), "%s\n%lld\n%lld\n%o\n%lld\n", kind,
                        (long long)info.st_size,
                        (long long)info.st_mtime * 1000LL,
                        (unsigned int)(info.st_mode & 07777),
                        (long long)info.st_ctime * 1000LL);
  moonbit_bytes_t result = moonbit_make_bytes(length, 0);
  memcpy(result, buffer, (size_t)length);
  return result;
#else
  return mulp_vinyl_fs_metadata_bytes((const char *)path, 1);
#endif
}

MOONBIT_FFI_EXPORT
int32_t mulp_vinyl_fs_remove_path_for_test(moonbit_bytes_t path) {
#if defined(_WIN32) || defined(_WIN64)
  return remove((const char *)path) == 0;
#else
  return unlink((const char *)path) == 0;
#endif
}

MOONBIT_FFI_EXPORT
int32_t mulp_vinyl_fs_path_exists_for_test(moonbit_bytes_t path) {
  struct stat info;
  return stat((const char *)path, &info) == 0;
}

MOONBIT_FFI_EXPORT
int32_t mulp_vinyl_fs_path_exists(moonbit_bytes_t path) {
  struct stat info;
#if defined(_WIN32) || defined(_WIN64)
  return stat((const char *)path, &info) == 0;
#else
  return lstat((const char *)path, &info) == 0;
#endif
}

MOONBIT_FFI_EXPORT
MulpVinylFsDestWriter *mulp_vinyl_fs_open_dest_writer(moonbit_bytes_t path,
                                                      int32_t append,
                                                      int32_t overwrite) {
  MulpVinylFsDestWriter *writer =
      (MulpVinylFsDestWriter *)moonbit_make_external_object(
          mulp_vinyl_fs_dest_writer_finalize, sizeof(MulpVinylFsDestWriter));
  writer->file = NULL;
  writer->skipped = 0;
  const char *path_text = (const char *)path;
  struct stat info;
  if (!overwrite && stat(path_text, &info) == 0) {
    writer->skipped = 1;
    return writer;
  }
  if (!mulp_vinyl_fs_ensure_parent_dirs(path_text)) {
    return writer;
  }
  writer->file = fopen(path_text, append ? "ab" : "wb");
  return writer;
}

MOONBIT_FFI_EXPORT
int32_t mulp_vinyl_fs_dest_writer_is_ready(MulpVinylFsDestWriter *writer) {
  return writer->skipped || writer->file != NULL;
}

MOONBIT_FFI_EXPORT
int32_t mulp_vinyl_fs_dest_writer_is_skipped(MulpVinylFsDestWriter *writer) {
  return writer->skipped;
}

MOONBIT_FFI_EXPORT
int32_t mulp_vinyl_fs_dest_writer_write(MulpVinylFsDestWriter *writer,
                                        moonbit_bytes_t chunk) {
  if (writer->skipped) {
    return 1;
  }
  if (writer->file == NULL) {
    return 0;
  }
  int32_t length = Moonbit_array_length(chunk);
  return fwrite(chunk, 1, (size_t)length, writer->file) == (size_t)length;
}

MOONBIT_FFI_EXPORT
int32_t mulp_vinyl_fs_dest_writer_close(MulpVinylFsDestWriter *writer) {
  if (writer->file == NULL) {
    return 1;
  }
  int result = fclose(writer->file) == 0;
  writer->file = NULL;
  return result;
}

MOONBIT_FFI_EXPORT
int32_t mulp_vinyl_fs_chmod_path(moonbit_bytes_t path, int32_t mode) {
#if defined(_WIN32) || defined(_WIN64)
  return _chmod((const char *)path, mode) == 0;
#else
  return chmod((const char *)path, (mode_t)mode) == 0;
#endif
}

MOONBIT_FFI_EXPORT
int32_t mulp_vinyl_fs_set_mtime_path(moonbit_bytes_t path, int64_t mtime_ms) {
  struct stat info;
  if (stat((const char *)path, &info) != 0) {
    return 0;
  }
#if defined(_WIN32) || defined(_WIN64)
  struct _utimbuf times;
  times.actime = info.st_atime;
  times.modtime = (time_t)(mtime_ms / 1000);
  return _utime((const char *)path, &times) == 0;
#else
  struct utimbuf times;
  times.actime = info.st_atime;
  times.modtime = (time_t)(mtime_ms / 1000);
  return utime((const char *)path, &times) == 0;
#endif
}

static void mulp_vinyl_fs_dest_writer_finalize(void *ptr) {
  (void)mulp_vinyl_fs_dest_writer_close((MulpVinylFsDestWriter *)ptr);
}

static int mulp_vinyl_fs_mkdir_if_needed(char *path) {
  if (path[0] == '\0') {
    return 1;
  }
  if (MULP_VINYL_FS_MKDIR(path) == 0) {
    return 1;
  }
  return errno == EEXIST;
}

static int mulp_vinyl_fs_ensure_parent_dirs(const char *path) {
  size_t length = strlen(path);
  char *copy = (char *)malloc(length + 1);
  if (copy == NULL) {
    return 0;
  }
  memcpy(copy, path, length + 1);
  for (size_t index = 1; index < length; index++) {
    if (copy[index] == '/' || copy[index] == '\\') {
      char saved = copy[index];
      copy[index] = '\0';
      if (!mulp_vinyl_fs_mkdir_if_needed(copy)) {
        free(copy);
        return 0;
      }
      copy[index] = saved;
    }
  }
  free(copy);
  return 1;
}
