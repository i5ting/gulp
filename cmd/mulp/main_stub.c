#include <moonbit.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

MOONBIT_FFI_EXPORT
void mulp_write_stdout(moonbit_bytes_t text) {
  int32_t length = Moonbit_array_length(text);
  fwrite(text, 1, (size_t)length, stdout);
}

MOONBIT_FFI_EXPORT
void mulp_write_stderr(moonbit_bytes_t text) {
  int32_t length = Moonbit_array_length(text);
  fwrite(text, 1, (size_t)length, stderr);
}

MOONBIT_FFI_EXPORT
void mulp_exit_process(int32_t status) {
  exit(status);
}

#include <signal.h>
#include <time.h>

static volatile int mulp_sigint_flag = 0;

static void mulp_sigint_handler(int sig) {
  (void)sig;
  mulp_sigint_flag = 1;
}

MOONBIT_FFI_EXPORT
void mulp_register_sigint(void) {
  struct sigaction sa;
  sa.sa_handler = mulp_sigint_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, NULL);
}

MOONBIT_FFI_EXPORT
int32_t mulp_check_sigint(void) {
  return mulp_sigint_flag ? 1 : 0;
}

MOONBIT_FFI_EXPORT
void mulp_sleep_ms(int32_t ms) {
  struct timespec ts;
  ts.tv_sec  = (time_t)(ms / 1000);
  ts.tv_nsec = (long)((ms % 1000) * 1000000L);
  nanosleep(&ts, NULL);
}
