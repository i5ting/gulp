#include <stdio.h>
#include <time.h>

void chokidar_cli_init(void) {
  setvbuf(stdout, NULL, _IOLBF, 0);
}

void chokidar_cli_sleep_ms(int ms) {
  if (ms <= 0) {
    return;
  }
  struct timespec req;
  req.tv_sec = ms / 1000;
  req.tv_nsec = (long)(ms % 1000) * 1000000L;
  nanosleep(&req, NULL);
}
