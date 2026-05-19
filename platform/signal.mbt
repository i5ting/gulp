///|
pub(all) enum PlatformSignal {
  Interrupt
  Terminate
}

///|
pub struct SignalWatcher {
  cancellation : @core.CancellationToken
}

///|
pub fn signal_watcher(cancellation : @core.CancellationToken) -> SignalWatcher {
  install_platform_signal_handlers()
  { cancellation, }
}

///|
pub fn SignalWatcher::poll(self : SignalWatcher) -> PlatformSignal? {
  match take_platform_signal() {
    2 => {
      self.cancellation.cancel()
      Some(Interrupt)
    }
    15 => {
      self.cancellation.cancel()
      Some(Terminate)
    }
    _ => None
  }
}

///|
extern "c" fn install_platform_signal_handlers() -> Unit = "mulp_install_signal_handlers"

///|
extern "c" fn take_platform_signal() -> Int = "mulp_take_signal"
