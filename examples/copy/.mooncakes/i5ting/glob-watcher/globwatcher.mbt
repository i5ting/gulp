// ── GlobWatcher ───────────────────────────────────────────────────────────────

priv struct ListenerEntry {
  event    : @chokidar.EventName
  callback : (@chokidar.Event) -> Unit
}

///|
pub struct GlobWatcher {
  priv chokidar_watcher : @chokidar.Watcher
  priv debouncer        : Debouncer
  priv queue            : CallbackQueue
  priv positives        : Array[GlobPattern]
  priv options          : Options
  priv listeners        : Array[ListenerEntry]
}

///| Create a watcher for the given glob patterns.
/// Raises GlobError if patterns are invalid, SetupError if chokidar fails.
pub fn GlobWatcher::new(
  glob    : @chokidar.WatchPaths,
  options : Options,
) -> GlobWatcher raise GlobWatcherError {
  let raw_patterns = match glob {
    One(p)   => [p]
    Many(ps) => ps.copy()
  }

  let (positives, final_negations) = parse_globs(raw_patterns)

  if positives.is_empty() {
    raise GlobError("no positive glob patterns provided")
  }

  // Build ignored list: user-supplied + negation globs
  let negation_matchers : Array[@chokidar.Matcher] = final_negations.map(fn(g) {
    @chokidar.Matcher::MatchFunction(fn(path, _) { glob_match(g.raw, path) })
  })
  let all_ignored = options.ignored + negation_matchers

  let chokidar_opts = @chokidar.Options::{
    ..@chokidar.Options::default(),
    ignore_initial: options.ignore_initial,
    ignored:        all_ignored,
    cwd:            options.cwd,
    depth:          options.depth,
    persistent:     false,
    use_polling:    options.use_polling,
  }

  // Deduplicate base dirs
  let seen : Array[String] = []
  let base_dirs : Array[String] = []
  for g in positives {
    if !seen.contains(g.base_dir) {
      seen.push(g.base_dir)
      base_dirs.push(g.base_dir)
    }
  }

  // Resolve base_dirs against cwd so the native scanner operates in the right
  // directory (chokidar's cwd option only affects path formatting, not where
  // readdirp actually scans from).
  let resolved_dirs = match options.cwd {
    None => base_dirs
    Some(cwd) =>
      base_dirs.map(fn(d) { if d == "." { cwd } else { cwd + "/" + d } })
  }

  let watch_paths = if resolved_dirs.length() == 1 {
    @chokidar.WatchPaths::One(resolved_dirs[0])
  } else {
    @chokidar.WatchPaths::Many(resolved_dirs)
  }

  let chokidar_watcher = @chokidar.watch(watch_paths, options=chokidar_opts) catch {
    @chokidar.ChokidarError(msg) => raise SetupError(msg)
  }

  {
    chokidar_watcher,
    debouncer: Debouncer::new(options.delay),
    queue:     CallbackQueue::new(options.queue),
    positives,
    options,
    listeners: [],
  }
}

///| Register a listener for a chokidar event (Ready, Error, Raw, etc.).
/// Returns self for chaining.
pub fn GlobWatcher::on(
  self     : GlobWatcher,
  event    : @chokidar.EventName,
  callback : (@chokidar.Event) -> Unit,
) -> GlobWatcher {
  self.listeners.push({ event, callback })
  self
}

///| Drive one process + debounce + queue cycle (non-blocking).
/// Used by tests and custom loop implementations.
pub fn GlobWatcher::tick(
  self : GlobWatcher,
  cb?  : (@chokidar.Event) -> Unit raise Error,
) -> Unit raise GlobWatcherError {
  let file_events   : Array[@chokidar.Event] = []
  let direct_events : Array[@chokidar.Event] = []

  // Collect events from this process() call via a temporary listener.
  let handler : (@chokidar.Event) -> Unit = fn(event) {
    match event.name {
      @chokidar.EventName::Ready
      | @chokidar.EventName::Error
      | @chokidar.EventName::Raw => direct_events.push(event)
      _ =>
        if self.options.events.contains(event.name) &&
           self.positives.any(fn(p) { glob_match(p.raw, event.path) }) {
          file_events.push(event)
        }
    }
  }

  // Register for All (file events) AND explicitly for Ready/Error/Raw since
  // the chokidar emitter does not forward those to All listeners.
  self.chokidar_watcher.on(@chokidar.EventName::All, handler) |> ignore
  self.chokidar_watcher.on(@chokidar.EventName::Ready, handler) |> ignore
  self.chokidar_watcher.on(@chokidar.EventName::Error, handler) |> ignore
  self.chokidar_watcher.on(@chokidar.EventName::Raw, handler) |> ignore

  let process_err = try {
    self.chokidar_watcher.process()
    None
  } catch {
    @chokidar.ChokidarError(msg) => Some(msg)
  }

  self.chokidar_watcher.off(@chokidar.EventName::All, handler) |> ignore
  self.chokidar_watcher.off(@chokidar.EventName::Ready, handler) |> ignore
  self.chokidar_watcher.off(@chokidar.EventName::Error, handler) |> ignore
  self.chokidar_watcher.off(@chokidar.EventName::Raw, handler) |> ignore

  // Forward Ready / Error / Raw to registered listeners.
  for event in direct_events {
    emit_to_listeners(self.listeners, event)
  }

  // Surface process errors as Error events.
  match process_err {
    Some(msg) =>
      emit_to_listeners(
        self.listeners,
        {
          name:  @chokidar.EventName::Error,
          path:  "",
          stats: None,
          raw:   None,
          error: Some(@chokidar.ChokidarError(msg)),
        },
      )
    None => ()
  }

  // Feed file events into the debouncer.
  let now = @env.now()
  for event in file_events {
    self.debouncer.feed(event, now)
  }

  // Flush and run callback when debounce window has elapsed.
  let now2 = @env.now()
  match self.debouncer.flush(now2) {
    Some(event) =>
      match cb {
        Some(callback) =>
          self.queue.try_run(event, callback, fn(msg) {
            emit_to_listeners(
              self.listeners,
              {
                name:  @chokidar.EventName::Error,
                path:  "",
                stats: None,
                raw:   None,
                error: Some(@chokidar.ChokidarError(msg)),
              },
            )
          })
        None => ()
      }
    None => ()
  }
}

///| Block in a polling loop until SIGINT/SIGTERM terminates the process.
pub fn GlobWatcher::run(
  self : GlobWatcher,
  cb?  : (@chokidar.Event) -> Unit raise Error,
) -> Unit raise GlobWatcherError {
  while true {
    self.tick(cb?)
    gw_sleep_ms(self.options.loop_interval)
  }
}

///| Stop watching and release resources.
pub fn GlobWatcher::close(self : GlobWatcher) -> Unit {
  self.chokidar_watcher.close()
  self.listeners.clear()
}

///| Return a snapshot of the currently watched paths.
pub fn GlobWatcher::get_watched(
  self : GlobWatcher,
) -> Map[String, Array[String]] {
  self.chokidar_watcher.get_watched()
}

// ── High-level entry point ────────────────────────────────────────────────────

///| Watch glob patterns and invoke `cb` on each debounced change.
/// Blocks until SIGINT/SIGTERM.
pub fn watch(
  glob    : @chokidar.WatchPaths,
  options : Options,
  cb      : (@chokidar.Event) -> Unit raise Error,
) -> Unit raise GlobWatcherError {
  let w = GlobWatcher::new(glob, options)
  w.run(cb=cb)
}

// ── Helpers ───────────────────────────────────────────────────────────────────

fn emit_to_listeners(
  listeners : Array[ListenerEntry],
  event     : @chokidar.Event,
) -> Unit {
  for entry in listeners {
    if entry.event == event.name || entry.event == @chokidar.EventName::All {
      (entry.callback)(event)
    }
  }
}

extern "C" fn gw_sleep_ms(ms : Int) -> Unit = "globwatcher_sleep_ms"
