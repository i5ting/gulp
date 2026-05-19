#include "moonbit.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

static char *gw_cstring_from_bytes(moonbit_bytes_t bytes) {
  size_t len = bytes == NULL ? 0 : (size_t)Moonbit_array_length(bytes);
  char *text = (char *)malloc(len + 1);
  if (text == NULL) return NULL;
  if (len > 0) memcpy(text, bytes, len);
  text[len] = '\0';
  return text;
}

MOONBIT_FFI_EXPORT void
globwatcher_sleep_ms(int ms) {
  usleep((useconds_t)(ms * 1000));
}

MOONBIT_FFI_EXPORT int
globwatcher_test_mkdir(moonbit_bytes_t path) {
  char *p = gw_cstring_from_bytes(path);
  if (p == NULL) return -1;
  int r = mkdir(p, 0755);
  free(p);
  return (r == 0 || errno == EEXIST) ? 0 : -1;
}

MOONBIT_FFI_EXPORT int
globwatcher_test_write_file(moonbit_bytes_t path, moonbit_bytes_t contents) {
  char *p = gw_cstring_from_bytes(path);
  if (p == NULL) return -1;
  size_t clen = contents == NULL ? 0 : (size_t)Moonbit_array_length(contents);
  FILE *f = fopen(p, "w");
  free(p);
  if (f == NULL) return -1;
  if (clen > 0) fwrite(contents, 1, clen, f);
  fclose(f);
  return 0;
}

MOONBIT_FFI_EXPORT int
globwatcher_test_remove_tree(moonbit_bytes_t path) {
  char *p = gw_cstring_from_bytes(path);
  if (p == NULL) return -1;
  char cmd[4096];
  snprintf(cmd, sizeof(cmd), "rm -rf '%s'", p);
  free(p);
  return system(cmd);
}
