#include "moonbit.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define CHOKIDAR_PLATFORM "win32"
#elif defined(__APPLE__)
#define CHOKIDAR_PLATFORM "darwin"
#elif defined(__linux__)
#define CHOKIDAR_PLATFORM "linux"
#elif defined(__FreeBSD__)
#define CHOKIDAR_PLATFORM "freebsd"
#elif defined(__OS400__)
#define CHOKIDAR_PLATFORM "os400"
#else
#define CHOKIDAR_PLATFORM "unknown"
#endif

/* ------------------------------------------------------------------ *
 * Event queue
 * ------------------------------------------------------------------ */

typedef struct {
  char *name;
  char *watched;
  char *path;
} chokidar_event_t;

static chokidar_event_t chokidar_events[1024];
static int chokidar_event_len = 0;
static int chokidar_next_handle = 1;
static int chokidar_event_overflow = 0;

static char *chokidar_backend_cstring_from_bytes(moonbit_bytes_t bytes) {
  size_t len = bytes == NULL ? 0 : (size_t)Moonbit_array_length(bytes);
  char *text = (char *)malloc(len + 1);
  if (text == NULL) return NULL;
  if (len > 0) memcpy(text, bytes, len);
  text[len] = '\0';
  return text;
}

static char *chokidar_backend_strdup_cstring(const char *text) {
  if (text == NULL) text = "";
  size_t len = strlen(text);
  char *copy = (char *)malloc(len + 1);
  if (copy == NULL) return NULL;
  if (len > 0) memcpy(copy, text, len);
  copy[len] = '\0';
  return copy;
}

static moonbit_bytes_t chokidar_backend_bytes_from_cstring(const char *text) {
  if (text == NULL) text = "";
  size_t len = strlen(text);
  moonbit_bytes_t bytes = moonbit_make_bytes((int32_t)len, 0);
  if (len > 0) memcpy(bytes, text, len);
  return bytes;
}

/* Defined before native poll so it can be called from there */
static void chokidar_backend_enqueue_raw_event_cstrings(
    const char *name, const char *watched, const char *path) {
  if (chokidar_event_len >= 1024) {
    chokidar_event_overflow = 1;
    return;
  }
  chokidar_events[chokidar_event_len].name =
      chokidar_backend_strdup_cstring(name);
  chokidar_events[chokidar_event_len].watched =
      chokidar_backend_strdup_cstring(watched);
  chokidar_events[chokidar_event_len].path =
      chokidar_backend_strdup_cstring(path);
  if (chokidar_events[chokidar_event_len].name == NULL ||
      chokidar_events[chokidar_event_len].watched == NULL ||
      chokidar_events[chokidar_event_len].path == NULL) {
    free(chokidar_events[chokidar_event_len].name);
    free(chokidar_events[chokidar_event_len].watched);
    free(chokidar_events[chokidar_event_len].path);
    chokidar_events[chokidar_event_len].name = NULL;
    chokidar_events[chokidar_event_len].watched = NULL;
    chokidar_events[chokidar_event_len].path = NULL;
    return;
  }
  chokidar_event_len += 1;
}

/* ------------------------------------------------------------------ *
 * Native watch table
 * ------------------------------------------------------------------ */

#define CHOKIDAR_MAX_WATCHES 256

typedef struct {
  int id;
  int fd;    /* kqueue vnode fd or inotify watch descriptor; -1 for polling */
  char *path;
} chokidar_watch_t;

static chokidar_watch_t chokidar_watches[CHOKIDAR_MAX_WATCHES];
static int chokidar_watch_count = 0;

/* ------------------------------------------------------------------ *
 * Platform: kqueue (macOS, FreeBSD)
 * ------------------------------------------------------------------ */

#if defined(__APPLE__) || defined(__FreeBSD__)

#include <fcntl.h>
#include <sys/event.h>
#include <unistd.h>

static int chokidar_kqueue_fd = -1;

static int chokidar_find_watch_by_fd(int fd) {
  for (int i = 0; i < chokidar_watch_count; i++) {
    if (chokidar_watches[i].fd == fd) return i;
  }
  return -1;
}

static void chokidar_native_poll(void) {
  if (chokidar_kqueue_fd < 0) return;
  struct kevent events[64];
  struct timespec timeout = {0, 0};
  int n = kevent(chokidar_kqueue_fd, NULL, 0, events, 64, &timeout);
  for (int i = 0; i < n; i++) {
    if (events[i].filter != EVFILT_VNODE) continue;
    int idx = chokidar_find_watch_by_fd((int)events[i].ident);
    if (idx >= 0) {
      chokidar_backend_enqueue_raw_event_cstrings(
          "change", chokidar_watches[idx].path, "");
    }
  }
}

static int chokidar_native_watch_path(int id, const char *path) {
  if (chokidar_kqueue_fd < 0) {
    chokidar_kqueue_fd = kqueue();
    if (chokidar_kqueue_fd < 0) return -1;
  }
#ifdef __APPLE__
  int fd = open(path, O_RDONLY | O_EVTONLY);
#else
  int fd = open(path, O_RDONLY);
#endif
  if (fd < 0) return -1;
  struct kevent change;
  EV_SET(&change, fd, EVFILT_VNODE, EV_ADD | EV_CLEAR,
         NOTE_WRITE | NOTE_DELETE | NOTE_RENAME | NOTE_ATTRIB | NOTE_EXTEND,
         0, NULL);
  if (kevent(chokidar_kqueue_fd, &change, 1, NULL, 0, NULL) < 0) {
    close(fd);
    return -1;
  }
  if (chokidar_watch_count >= CHOKIDAR_MAX_WATCHES) {
    close(fd);
    return -1;
  }
  chokidar_watches[chokidar_watch_count].id = id;
  chokidar_watches[chokidar_watch_count].fd = fd;
  chokidar_watches[chokidar_watch_count].path =
      chokidar_backend_strdup_cstring(path);
  chokidar_watch_count++;
  return id;
}

static void chokidar_native_close_id(int id) {
  for (int i = 0; i < chokidar_watch_count; i++) {
    if (chokidar_watches[i].id != id) continue;
    if (chokidar_watches[i].fd >= 0 && chokidar_kqueue_fd >= 0) {
      struct kevent change;
      EV_SET(&change, chokidar_watches[i].fd, EVFILT_VNODE, EV_DELETE, 0, 0,
             NULL);
      kevent(chokidar_kqueue_fd, &change, 1, NULL, 0, NULL);
      close(chokidar_watches[i].fd);
    }
    free(chokidar_watches[i].path);
    for (int j = i; j < chokidar_watch_count - 1; j++) {
      chokidar_watches[j] = chokidar_watches[j + 1];
    }
    chokidar_watch_count--;
    return;
  }
}

/* ------------------------------------------------------------------ *
 * Platform: inotify (Linux)
 * ------------------------------------------------------------------ */

#elif defined(__linux__)

#include <sys/inotify.h>
#include <unistd.h>

static int chokidar_inotify_fd = -1;

#define CHOKIDAR_INOTIFY_MASK                                                  \
  (IN_CREATE | IN_MODIFY | IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF |         \
   IN_MOVED_FROM | IN_MOVED_TO | IN_ATTRIB | IN_CLOSE_WRITE)

static int chokidar_find_watch_by_wd(int wd) {
  for (int i = 0; i < chokidar_watch_count; i++) {
    if (chokidar_watches[i].fd == wd) return i;
  }
  return -1;
}

static void chokidar_native_poll(void) {
  if (chokidar_inotify_fd < 0) return;
  char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
  ssize_t len;
  while ((len = read(chokidar_inotify_fd, buf, sizeof(buf))) > 0) {
    const char *ptr = buf;
    while (ptr < buf + len) {
      const struct inotify_event *ev = (const struct inotify_event *)ptr;
      int idx = chokidar_find_watch_by_wd(ev->wd);
      if (idx >= 0) {
        const char *name = ev->len > 0 ? ev->name : "";
        chokidar_backend_enqueue_raw_event_cstrings(
            "change", chokidar_watches[idx].path, name);
      }
      ptr += sizeof(struct inotify_event) + ev->len;
    }
  }
}

static int chokidar_native_watch_path(int id, const char *path) {
  if (chokidar_inotify_fd < 0) {
    chokidar_inotify_fd = inotify_init1(IN_NONBLOCK);
    if (chokidar_inotify_fd < 0) return -1;
  }
  int wd = inotify_add_watch(chokidar_inotify_fd, path, CHOKIDAR_INOTIFY_MASK);
  if (wd < 0) return -1;
  if (chokidar_watch_count >= CHOKIDAR_MAX_WATCHES) {
    inotify_rm_watch(chokidar_inotify_fd, wd);
    return -1;
  }
  chokidar_watches[chokidar_watch_count].id = id;
  chokidar_watches[chokidar_watch_count].fd = wd;
  chokidar_watches[chokidar_watch_count].path =
      chokidar_backend_strdup_cstring(path);
  chokidar_watch_count++;
  return id;
}

static void chokidar_native_close_id(int id) {
  for (int i = 0; i < chokidar_watch_count; i++) {
    if (chokidar_watches[i].id != id) continue;
    if (chokidar_watches[i].fd >= 0 && chokidar_inotify_fd >= 0) {
      inotify_rm_watch(chokidar_inotify_fd, chokidar_watches[i].fd);
    }
    free(chokidar_watches[i].path);
    for (int j = i; j < chokidar_watch_count - 1; j++) {
      chokidar_watches[j] = chokidar_watches[j + 1];
    }
    chokidar_watch_count--;
    return;
  }
}

/* ------------------------------------------------------------------ *
 * Fallback: no native watching
 * ------------------------------------------------------------------ */

#else

static void chokidar_native_poll(void) {}

static int chokidar_native_watch_path(int id, const char *path) {
  (void)path;
  return id;
}

static void chokidar_native_close_id(int id) { (void)id; }

#endif

/* ------------------------------------------------------------------ *
 * Exported MoonBit FFI functions
 * ------------------------------------------------------------------ */

MOONBIT_FFI_EXPORT int
chokidar_backend_watch(moonbit_bytes_t path, int use_polling, int interval) {
  (void)interval;
  int id = chokidar_next_handle++;
  if (use_polling) return id;
  char *path_text = chokidar_backend_cstring_from_bytes(path);
  if (path_text == NULL) return id;
  int result = chokidar_native_watch_path(id, path_text);
  free(path_text);
  return result < 0 ? -1 : id;
}

MOONBIT_FFI_EXPORT void chokidar_backend_close(int id) {
  chokidar_native_close_id(id);
}

MOONBIT_FFI_EXPORT int chokidar_backend_event_count(void) {
  chokidar_native_poll();
  return chokidar_event_len + (chokidar_event_overflow ? 1 : 0);
}

MOONBIT_FFI_EXPORT moonbit_bytes_t chokidar_backend_event_name(int index) {
  if (index == chokidar_event_len && chokidar_event_overflow) {
    return chokidar_backend_bytes_from_cstring("overflow");
  }
  if (index < 0 || index >= chokidar_event_len) {
    return chokidar_backend_bytes_from_cstring("");
  }
  return chokidar_backend_bytes_from_cstring(chokidar_events[index].name);
}

MOONBIT_FFI_EXPORT moonbit_bytes_t chokidar_backend_watched_path(int index) {
  if (index == chokidar_event_len && chokidar_event_overflow) {
    return chokidar_backend_bytes_from_cstring("");
  }
  if (index < 0 || index >= chokidar_event_len) {
    return chokidar_backend_bytes_from_cstring("");
  }
  return chokidar_backend_bytes_from_cstring(chokidar_events[index].watched);
}

MOONBIT_FFI_EXPORT moonbit_bytes_t chokidar_backend_event_path(int index) {
  if (index == chokidar_event_len && chokidar_event_overflow) {
    return chokidar_backend_bytes_from_cstring("");
  }
  if (index < 0 || index >= chokidar_event_len) {
    return chokidar_backend_bytes_from_cstring("");
  }
  return chokidar_backend_bytes_from_cstring(chokidar_events[index].path);
}

MOONBIT_FFI_EXPORT moonbit_bytes_t chokidar_backend_platform(void) {
  return chokidar_backend_bytes_from_cstring(CHOKIDAR_PLATFORM);
}

MOONBIT_FFI_EXPORT int
chokidar_getenv_valid(moonbit_bytes_t name) {
  char *name_text = chokidar_backend_cstring_from_bytes(name);
  if (name_text == NULL) return 0;
  int ok = getenv(name_text) != NULL ? 1 : 0;
  free(name_text);
  return ok;
}

MOONBIT_FFI_EXPORT moonbit_bytes_t
chokidar_getenv_value(moonbit_bytes_t name) {
  char *name_text = chokidar_backend_cstring_from_bytes(name);
  if (name_text == NULL) return chokidar_backend_bytes_from_cstring("");
  const char *val = getenv(name_text);
  free(name_text);
  if (val == NULL) return chokidar_backend_bytes_from_cstring("");
  return chokidar_backend_bytes_from_cstring(val);
}

MOONBIT_FFI_EXPORT void chokidar_backend_clear_events(void) {
  for (int i = 0; i < chokidar_event_len; i += 1) {
    free(chokidar_events[i].name);
    free(chokidar_events[i].watched);
    free(chokidar_events[i].path);
    chokidar_events[i].name = NULL;
    chokidar_events[i].watched = NULL;
    chokidar_events[i].path = NULL;
  }
  chokidar_event_len = 0;
  chokidar_event_overflow = 0;
}

MOONBIT_FFI_EXPORT void chokidar_backend_enqueue_raw_event(
    moonbit_bytes_t name, moonbit_bytes_t watched, moonbit_bytes_t path) {
  char *name_text = chokidar_backend_cstring_from_bytes(name);
  char *watched_text = chokidar_backend_cstring_from_bytes(watched);
  char *path_text = chokidar_backend_cstring_from_bytes(path);
  if (name_text == NULL || watched_text == NULL || path_text == NULL) {
    free(name_text);
    free(watched_text);
    free(path_text);
    return;
  }
  chokidar_backend_enqueue_raw_event_cstrings(name_text, watched_text,
                                              path_text);
  free(name_text);
  free(watched_text);
  free(path_text);
}
