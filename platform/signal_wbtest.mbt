///|
fn raise_platform_signal_for_test(signal : PlatformSignal) -> Unit {
  let code = match signal {
    Interrupt => 2
    Terminate => 15
  }
  raise_platform_signal_for_test_ffi(code)
}

///|
extern "c" fn raise_platform_signal_for_test_ffi(code : Int) -> Unit = "mulp_raise_signal_for_test"

///|
test "signal watcher turns interrupt into cancellation" {
  let token = @core.new_cancellation_token()
  let watcher = signal_watcher(token)
  raise_platform_signal_for_test(Interrupt)
  inspect(watcher.poll() is Some(Interrupt), content="true")
  inspect(token.is_cancelled(), content="true")
  inspect(watcher.poll() is None, content="true")
}
