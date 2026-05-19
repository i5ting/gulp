#include <moonbit.h>

#include <stdint.h>
#include <signal.h>

static volatile sig_atomic_t mulp_signal_code = 0;

static void mulp_signal_handler(int code) {
  mulp_signal_code = code;
}

MOONBIT_FFI_EXPORT
void mulp_install_signal_handlers(void) {
  signal(SIGINT, mulp_signal_handler);
  signal(SIGTERM, mulp_signal_handler);
}

MOONBIT_FFI_EXPORT
int32_t mulp_take_signal(void) {
  int32_t code = (int32_t)mulp_signal_code;
  mulp_signal_code = 0;
  return code;
}

MOONBIT_FFI_EXPORT
void mulp_raise_signal_for_test(int32_t code) {
  raise(code);
}
