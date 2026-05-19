#include <moonbit.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#include <io.h>
#define MULP_CLOSE _close
#define MULP_TEMP_NAME(buffer, prefix) tmpnam(buffer)
#define MULP_UNLINK remove
#else
#include <unistd.h>
#include <sys/wait.h>
#define MULP_CLOSE close
#define MULP_UNLINK unlink
#endif

static int32_t mulp_exit_code(int status) {
#if defined(_WIN32) || defined(_WIN64)
  return status;
#else
  if (status == -1) {
    return 127;
  }
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  return 128;
#endif
}

static void mulp_append(char **buffer, int32_t *length, int32_t *capacity,
                        const char *chunk, int32_t chunk_length) {
  if (*length + chunk_length > *capacity) {
    int32_t next_capacity = *capacity == 0 ? 4096 : *capacity;
    while (*length + chunk_length > next_capacity) {
      next_capacity *= 2;
    }
    char *next = (char *)realloc(*buffer, (size_t)next_capacity);
    if (next == NULL) {
      free(*buffer);
      *buffer = NULL;
      *length = 0;
      *capacity = 0;
      return;
    }
    *buffer = next;
    *capacity = next_capacity;
  }
  memcpy(*buffer + *length, chunk, (size_t)chunk_length);
  *length += chunk_length;
}

static int mulp_make_temp_file(char *buffer, const char *prefix) {
#if defined(_WIN32) || defined(_WIN64)
  (void)prefix;
  return MULP_TEMP_NAME(buffer, prefix) == NULL ? -1 : 0;
#else
  snprintf(buffer, 256, "/tmp/%s-XXXXXX", prefix);
  int fd = mkstemp(buffer);
  if (fd == -1) {
    return -1;
  }
  MULP_CLOSE(fd);
  return 0;
#endif
}

static char *mulp_shell_quote(const char *value) {
  int32_t length = 2;
  for (const char *ptr = value; *ptr != '\0'; ptr++) {
    length += *ptr == '\'' ? 4 : 1;
  }
  char *quoted = (char *)malloc((size_t)length + 1);
  if (quoted == NULL) {
    return NULL;
  }
  int32_t index = 0;
  quoted[index++] = '\'';
  for (const char *ptr = value; *ptr != '\0'; ptr++) {
    if (*ptr == '\'') {
      memcpy(quoted + index, "'\\''", 4);
      index += 4;
    } else {
      quoted[index++] = *ptr;
    }
  }
  quoted[index++] = '\'';
  quoted[index] = '\0';
  return quoted;
}

static char *mulp_read_file(const char *path, int32_t *length) {
  FILE *file = fopen(path, "rb");
  char *contents = NULL;
  int32_t capacity = 0;
  *length = 0;
  if (file == NULL) {
    return NULL;
  }
  char chunk[4096];
  size_t read = 0;
  while ((read = fread(chunk, 1, sizeof(chunk), file)) > 0) {
    mulp_append(&contents, length, &capacity, chunk, (int32_t)read);
    if (contents == NULL) {
      break;
    }
  }
  fclose(file);
  return contents;
}

MOONBIT_FFI_EXPORT
moonbit_bytes_t mulp_run_shell(moonbit_bytes_t command) {
  char stdout_path[256];
  char stderr_path[256];
  char *stdout_quoted = NULL;
  char *stderr_quoted = NULL;
  char *shell_command = NULL;
  char *stdout_output = NULL;
  char *stderr_output = NULL;
  int32_t stdout_length = 0;
  int32_t stderr_length = 0;
  int32_t exit_code = 127;
  stdout_path[0] = '\0';
  stderr_path[0] = '\0';

  if (mulp_make_temp_file(stdout_path, "mulp-stdout") == 0 &&
      mulp_make_temp_file(stderr_path, "mulp-stderr") == 0) {
    stdout_quoted = mulp_shell_quote(stdout_path);
    stderr_quoted = mulp_shell_quote(stderr_path);
    if (stdout_quoted != NULL && stderr_quoted != NULL) {
      int32_t command_length =
          (int32_t)strlen((const char *)command) +
          (int32_t)strlen(stdout_quoted) + (int32_t)strlen(stderr_quoted) + 16;
      shell_command = (char *)malloc((size_t)command_length);
      if (shell_command != NULL) {
        snprintf(shell_command, (size_t)command_length, "(%s) > %s 2> %s",
                 (const char *)command, stdout_quoted, stderr_quoted);
        exit_code = mulp_exit_code(system(shell_command));
        stdout_output = mulp_read_file(stdout_path, &stdout_length);
        stderr_output = mulp_read_file(stderr_path, &stderr_length);
      }
    }
  }

  char prefix[32];
  int32_t prefix_length =
      (int32_t)snprintf(prefix, sizeof(prefix), "%d\n%d\n%d\n", exit_code,
                        stdout_length, stderr_length);
  moonbit_bytes_t result =
      moonbit_make_bytes(prefix_length + stdout_length + stderr_length, 0);
  memcpy(result, prefix, (size_t)prefix_length);
  if (stdout_length > 0) {
    memcpy(result + prefix_length, stdout_output, (size_t)stdout_length);
  }
  if (stderr_length > 0) {
    memcpy(result + prefix_length + stdout_length, stderr_output,
           (size_t)stderr_length);
  }

  if (stdout_path[0] != '\0') {
    MULP_UNLINK(stdout_path);
  }
  if (stderr_path[0] != '\0') {
    MULP_UNLINK(stderr_path);
  }
  free(stdout_quoted);
  free(stderr_quoted);
  free(shell_command);
  free(stdout_output);
  free(stderr_output);
  return result;
}
