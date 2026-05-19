#include <moonbit.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#include <direct.h>
#define MULP_MKDIR(path) _mkdir(path)
#define MULP_UNLINK(path) remove(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#define MULP_MKDIR(path) mkdir(path, 0777)
#define MULP_UNLINK(path) unlink(path)
#endif

typedef struct {
  FILE *file;
  int32_t chunk_size;
} MulpFileReader;

typedef struct {
  FILE *file;
} MulpFileWriter;

static int32_t mulp_open_file_reader_count = 0;
static int32_t mulp_open_file_writer_count = 0;

static int mulp_ensure_parent_dirs(const char *path);
static int mulp_mkdir_if_needed(char *path);
static void mulp_file_reader_close_impl(MulpFileReader *reader);
static int mulp_file_writer_close_impl(MulpFileWriter *writer);

static void mulp_file_reader_finalize(void *ptr) {
  mulp_file_reader_close_impl((MulpFileReader *)ptr);
}

static void mulp_file_writer_finalize(void *ptr) {
  (void)mulp_file_writer_close_impl((MulpFileWriter *)ptr);
}

MOONBIT_FFI_EXPORT
MulpFileReader *mulp_open_file_reader(moonbit_bytes_t path, int32_t chunk_size) {
  MulpFileReader *reader = (MulpFileReader *)moonbit_make_external_object(
      mulp_file_reader_finalize, sizeof(MulpFileReader));
  reader->file = fopen((const char *)path, "rb");
  reader->chunk_size = chunk_size;
  if (reader->file != NULL) {
    mulp_open_file_reader_count++;
  }
  return reader;
}

MOONBIT_FFI_EXPORT
int32_t mulp_file_reader_is_open(MulpFileReader *reader) {
  return reader->file != NULL;
}

MOONBIT_FFI_EXPORT
moonbit_bytes_t mulp_file_reader_read(MulpFileReader *reader) {
  if (reader->file == NULL) {
    return moonbit_make_bytes(0, 0);
  }
  moonbit_bytes_t bytes = moonbit_make_bytes(reader->chunk_size, 0);
  size_t read = fread(bytes, 1, (size_t)reader->chunk_size, reader->file);
  if (read == 0) {
    mulp_file_reader_close_impl(reader);
    return moonbit_make_bytes(0, 0);
  }
  if ((int32_t)read == reader->chunk_size) {
    return bytes;
  }
  moonbit_bytes_t trimmed = moonbit_make_bytes((int32_t)read, 0);
  for (int32_t index = 0; index < (int32_t)read; index++) {
    trimmed[index] = bytes[index];
  }
  return trimmed;
}

MOONBIT_FFI_EXPORT
void mulp_file_reader_close(MulpFileReader *reader) {
  mulp_file_reader_close_impl(reader);
}

MOONBIT_FFI_EXPORT
int32_t mulp_file_reader_open_count_for_test(void) {
  return mulp_open_file_reader_count;
}

MOONBIT_FFI_EXPORT
int32_t mulp_write_file_bytes(moonbit_bytes_t path, moonbit_bytes_t contents) {
  if (!mulp_ensure_parent_dirs((const char *)path)) {
    return 0;
  }
  FILE *file = fopen((const char *)path, "wb");
  if (file == NULL) {
    return 0;
  }
  int32_t length = Moonbit_array_length(contents);
  size_t written = fwrite(contents, 1, (size_t)length, file);
  int close_result = fclose(file);
  return written == (size_t)length && close_result == 0;
}

MOONBIT_FFI_EXPORT
MulpFileWriter *mulp_open_file_writer(moonbit_bytes_t path) {
  MulpFileWriter *writer = (MulpFileWriter *)moonbit_make_external_object(
      mulp_file_writer_finalize, sizeof(MulpFileWriter));
  if (!mulp_ensure_parent_dirs((const char *)path)) {
    writer->file = NULL;
  } else {
    writer->file = fopen((const char *)path, "wb");
    if (writer->file != NULL) {
      mulp_open_file_writer_count++;
    }
  }
  return writer;
}

MOONBIT_FFI_EXPORT
int32_t mulp_file_writer_is_open(MulpFileWriter *writer) {
  return writer->file != NULL;
}

MOONBIT_FFI_EXPORT
int32_t mulp_file_writer_write(MulpFileWriter *writer, moonbit_bytes_t chunk) {
  if (writer->file == NULL) {
    return 0;
  }
  int32_t length = Moonbit_array_length(chunk);
  return fwrite(chunk, 1, (size_t)length, writer->file) == (size_t)length;
}

MOONBIT_FFI_EXPORT
int32_t mulp_file_writer_close(MulpFileWriter *writer) {
  return mulp_file_writer_close_impl(writer);
}

MOONBIT_FFI_EXPORT
int32_t mulp_file_writer_open_count_for_test(void) {
  return mulp_open_file_writer_count;
}

MOONBIT_FFI_EXPORT
int32_t mulp_make_directory_path(moonbit_bytes_t path) {
  if (!mulp_ensure_parent_dirs((const char *)path)) {
    return 0;
  }
  char *copy = (char *)malloc(strlen((const char *)path) + 1);
  if (copy == NULL) {
    return 0;
  }
  strcpy(copy, (const char *)path);
  int result = mulp_mkdir_if_needed(copy);
  free(copy);
  return result;
}

MOONBIT_FFI_EXPORT
int32_t mulp_make_symlink_path(moonbit_bytes_t target, moonbit_bytes_t path) {
  if (!mulp_ensure_parent_dirs((const char *)path)) {
    return 0;
  }
  MULP_UNLINK((const char *)path);
#if defined(_WIN32) || defined(_WIN64)
  (void)target;
  return 0;
#else
  return symlink((const char *)target, (const char *)path) == 0;
#endif
}

MOONBIT_FFI_EXPORT
int32_t mulp_path_is_directory_for_test(moonbit_bytes_t path) {
  struct stat info;
  if (stat((const char *)path, &info) != 0) {
    return 0;
  }
  return S_ISDIR(info.st_mode);
}

MOONBIT_FFI_EXPORT
int32_t mulp_path_is_symlink_for_test(moonbit_bytes_t path) {
#if defined(_WIN32) || defined(_WIN64)
  (void)path;
  return 0;
#else
  struct stat info;
  if (lstat((const char *)path, &info) != 0) {
    return 0;
  }
  return S_ISLNK(info.st_mode);
#endif
}

MOONBIT_FFI_EXPORT
int32_t mulp_symlink_supported_for_test(void) {
#if defined(_WIN32) || defined(_WIN64)
  return 0;
#else
  return 1;
#endif
}

MOONBIT_FFI_EXPORT
moonbit_bytes_t mulp_file_metadata_bytes(moonbit_bytes_t path) {
  struct stat info;
#if defined(_WIN32) || defined(_WIN64)
  if (stat((const char *)path, &info) != 0) {
    return moonbit_make_bytes(0, 0);
  }
#else
  if (lstat((const char *)path, &info) != 0) {
    return moonbit_make_bytes(0, 0);
  }
#endif
  const char *kind = "other";
  if (S_ISREG(info.st_mode)) {
    kind = "file";
  } else if (S_ISDIR(info.st_mode)) {
    kind = "directory";
#if !defined(_WIN32) && !defined(_WIN64)
  } else if (S_ISLNK(info.st_mode)) {
    kind = "symlink";
#endif
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

static int mulp_mkdir_if_needed(char *path) {
  if (path[0] == '\0') {
    return 1;
  }
  if (MULP_MKDIR(path) == 0) {
    return 1;
  }
  return errno == EEXIST;
}

static int mulp_ensure_parent_dirs(const char *path) {
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
      if (!mulp_mkdir_if_needed(copy)) {
        free(copy);
        return 0;
      }
      copy[index] = saved;
    }
  }
  free(copy);
  return 1;
}

static void mulp_file_reader_close_impl(MulpFileReader *reader) {
  if (reader != NULL && reader->file != NULL) {
    fclose(reader->file);
    reader->file = NULL;
    mulp_open_file_reader_count--;
  }
}

static int mulp_file_writer_close_impl(MulpFileWriter *writer) {
  if (writer == NULL || writer->file == NULL) {
    return 1;
  }
  int result = fclose(writer->file);
  writer->file = NULL;
  mulp_open_file_writer_count--;
  return result == 0;
}
