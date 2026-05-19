///|
pub struct WatchOptions {
  delay          : Int
  queue          : Bool
  ignore_initial : Bool
  ignored        : Array[String]
  depth          : Int
  loop_interval  : Int
  cwd            : String?
  events         : Array[String]
  use_polling    : Bool
}

///|
pub fn default_watch_options() -> WatchOptions {
  {
    delay:          200,
    queue:          true,
    ignore_initial: true,
    ignored:        [],
    depth:          2147483647,
    loop_interval:  10,
    cwd:            None,
    events:         [],
    use_polling:    true,
  }
}

///|
fn event_name_from_string(s : String) -> @chokidar.EventName? {
  match s {
    "add"       => Some(@chokidar.EventName::Add)
    "change"    => Some(@chokidar.EventName::Change)
    "unlink"    => Some(@chokidar.EventName::Unlink)
    "addDir"    => Some(@chokidar.EventName::AddDir)
    "unlinkDir" => Some(@chokidar.EventName::UnlinkDir)
    "ready"     => Some(@chokidar.EventName::Ready)
    "error"     => Some(@chokidar.EventName::Error)
    "all"       => Some(@chokidar.EventName::All)
    _           => None
  }
}

///|
fn event_names_to_chokidar(
  events : Array[String],
) -> Array[@chokidar.EventName] {
  if events.is_empty() {
    @glob_watcher.Options::default().events
  } else {
    let result : Array[@chokidar.EventName] = []
    for s in events {
      match event_name_from_string(s) {
        Some(e) => result.push(e)
        None    => ()
      }
    }
    result
  }
}

///|
fn make_chok_options(options : WatchOptions) -> @glob_watcher.Options {
  let ignored_matchers : Array[@chokidar.Matcher] = options.ignored.map(
    fn(pat) {
      @chokidar.Matcher::MatchFunction(fn(path, _) {
        @glob_watcher.glob_match(pat, path)
      })
    },
  )
  {
    delay:          options.delay,
    queue:          options.queue,
    ignore_initial: options.ignore_initial,
    ignored:        ignored_matchers,
    depth:          options.depth,
    loop_interval:  options.loop_interval,
    cwd:            options.cwd,
    events:         event_names_to_chokidar(options.events),
    use_polling:    options.use_polling,
  }
}

///|
fn make_task_callback(
  task : @core.AsyncTask,
) -> (@chokidar.Event) -> Unit raise Error {
  fn(_event) {
    @async.run_async_main(async fn() {
      let token  = @core.new_cancellation_token()
      let now_ms = @env.now().reinterpret_as_int64()
      let ctx    = @core.new_context(cwd=".", now_ms~, cancellation=token)
      match task.execute(ctx) {
        Ok(_)  => ()
        Err(e) => println("watch task error: " + e.message())
      }
    })
  }
}

///|
fn globs_to_watch_paths(globs : Array[String]) -> @chokidar.WatchPaths {
  if globs.length() == 1 {
    @chokidar.WatchPaths::One(globs[0])
  } else {
    @chokidar.WatchPaths::Many(globs)
  }
}

///|
pub struct WatchHandle {
  priv watcher  : @glob_watcher.GlobWatcher
  priv callback : (@chokidar.Event) -> Unit raise Error
}

///|
pub fn WatchHandle::run(self : WatchHandle) -> Unit raise @glob_watcher.GlobWatcherError {
  self.watcher.run(cb=self.callback)
}

///|
pub fn WatchHandle::tick(self : WatchHandle) -> Unit raise @glob_watcher.GlobWatcherError {
  self.watcher.tick(cb=self.callback)
}

///|
pub fn WatchHandle::close(self : WatchHandle) -> Unit {
  self.watcher.close()
}

///|
pub fn WatchHandle::get_watched(self : WatchHandle) -> Map[String, Array[String]] {
  self.watcher.get_watched()
}

///|
/// Create a watcher without starting the event loop.
/// Call `.run()` to block, or `.tick()` in a loop for manual control.
pub fn watch_new(
  globs    : Array[String],
  task     : @core.AsyncTask,
  options? : WatchOptions = default_watch_options(),
) -> WatchHandle raise @glob_watcher.GlobWatcherError {
  let paths = globs_to_watch_paths(globs)
  let opts  = make_chok_options(options)
  let watcher = @glob_watcher.GlobWatcher::new(paths, opts)
  { watcher, callback: make_task_callback(task) }
}

///|
/// Watch glob patterns and re-run `task` on every debounced change.
/// Blocks until the process is terminated (or `@glob_watcher` raises).
///
/// Example in mulp.mbtx:
///   let build = async_task(registry, "build", fn(ctx) { ... })
///   watch(["src/**/*.mbt"], build)
pub fn watch(
  globs    : Array[String],
  task     : @core.AsyncTask,
  options? : WatchOptions = default_watch_options(),
) -> Unit raise Error {
  @glob_watcher.watch(
    globs_to_watch_paths(globs),
    make_chok_options(options),
    make_task_callback(task),
  )
}
