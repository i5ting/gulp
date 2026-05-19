// ── Shared type declarations ──────────────────────────────────────────────────
// These are compiled for all targets; native-specific implementations live in
// globwatcher.mbt (and stubs in globwatcher_stub.mbt for wasm/wasm-gc/js).

///|
pub(all) suberror GlobWatcherError {
  GlobError(String)
  SetupError(String)
} derive(Debug)

///|
pub(all) struct Options {
  /// Debounce delay in ms before invoking the callback. Default: 200.
  delay          : Int
  /// Queue the next run when callback is busy. Default: true.
  queue          : Bool
  /// Event types that trigger the callback. Default: [Add, Change, Unlink].
  events         : Array[@chokidar.EventName]
  /// Skip events for files that existed before watching. Default: true.
  ignore_initial : Bool
  /// Additional ignored matchers passed to chokidar.
  ignored        : Array[@chokidar.Matcher]
  /// Working directory for relative glob paths.
  cwd            : String?
  /// Maximum directory depth to watch.
  depth          : Int
  /// Polling interval for the blocking loop in ms. Default: 10.
  loop_interval  : Int
  /// Use polling instead of native fs events. Default: true.
  use_polling    : Bool
}

///|
pub fn Options::default() -> Options {
  {
    delay:          200,
    queue:          true,
    events:         [@chokidar.EventName::Add, Change, Unlink],
    ignore_initial: true,
    ignored:        [],
    cwd:            None,
    depth:          2147483647,
    loop_interval:  10,
    use_polling:    true,
  }
}

///|
pub(all) struct GlobPattern {
  raw         : String
  base_dir    : String
  is_negation : Bool
}
