// Stub implementations for wasm/wasm-gc/js targets.
// Real implementations are in globwatcher.mbt (native/llvm only).

///|
pub struct GlobWatcher {
  priv dummy : Unit
}

///|
pub fn GlobWatcher::new(
  _glob    : @chokidar.WatchPaths,
  _options : Options,
) -> GlobWatcher raise GlobWatcherError {
  { dummy: () }
}

///|
pub fn GlobWatcher::on(
  self      : GlobWatcher,
  _event    : @chokidar.EventName,
  _callback : (@chokidar.Event) -> Unit,
) -> GlobWatcher {
  self
}

///|
pub fn GlobWatcher::tick(
  self : GlobWatcher,
  cb?  : (@chokidar.Event) -> Unit raise Error,
) -> Unit raise GlobWatcherError {
  ignore(self)
  ignore(cb)
}

///|
pub fn GlobWatcher::run(
  self : GlobWatcher,
  cb?  : (@chokidar.Event) -> Unit raise Error,
) -> Unit raise GlobWatcherError {
  ignore(self)
  ignore(cb)
}

///|
pub fn GlobWatcher::close(_self : GlobWatcher) -> Unit {
  ()
}

///|
pub fn GlobWatcher::get_watched(
  _self : GlobWatcher,
) -> Map[String, Array[String]] {
  {}
}

///|
pub fn watch(
  _glob    : @chokidar.WatchPaths,
  _options : Options,
  _cb      : (@chokidar.Event) -> Unit raise Error,
) -> Unit raise GlobWatcherError {
  ()
}
