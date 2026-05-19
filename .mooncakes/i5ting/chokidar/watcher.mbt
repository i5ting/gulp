///|
pub struct Watcher {
  priv state : WatchState
  priv emitter : Emitter
}

///|
fn Watcher::new(options : Options) -> Watcher {
  { state: WatchState::new(options), emitter: Emitter::new() }
}

///|
fn ascii_lowercase(s : String) -> String {
  let mut result = ""
  for ch in s {
    let c = ch.to_int()
    let lower = if c >= 65 && c <= 90 { Int::unsafe_to_char(c + 32) } else { ch }
    result = result + lower.to_string()
  }
  result
}

///|
fn parse_int_option(s : String) -> Int? {
  if s == "" {
    return None
  }
  let mut result = 0
  for ch in s {
    let d = ch.to_int() - '0'.to_int()
    if d < 0 || d > 9 {
      return None
    }
    result = result * 10 + d
  }
  Some(result)
}

///|
fn apply_env_overrides(options : Options) -> Options {
  let mut opts = options
  match chokidar_getenv("CHOKIDAR_USEPOLLING") {
    Some(val) => {
      let lower = ascii_lowercase(val)
      if lower == "false" || lower == "0" {
        opts = { ..opts, use_polling: false }
      } else {
        opts = { ..opts, use_polling: lower != "" }
      }
    }
    None => ()
  }
  match chokidar_getenv("CHOKIDAR_INTERVAL") {
    Some(val) =>
      match parse_int_option(val) {
        Some(n) => opts = { ..opts, interval: n }
        None => ()
      }
    None => ()
  }
  opts
}

///|
pub fn watch(
  paths : WatchPaths,
  options? : Options = Options::default(),
) -> Watcher raise ChokidarError {
  let watcher = Watcher::new(apply_env_overrides(options))
  watcher.add(paths)
}

///|
pub fn Watcher::on(
  self : Watcher,
  event : EventName,
  callback : (Event) -> Unit,
) -> Watcher {
  self.emitter.on(event, callback)
  self
}

///|
priv struct PlannedAdd {
  root : String
  handle : BackendHandle
  entries : Array[SnapshotEntry]
  events : Array[Event]
}

///|
fn initial_events_from_entries(
  entries : Array[SnapshotEntry],
  options : Options,
) -> Array[Event] {
  if options.ignore_initial {
    []
  } else {
    entries.map(entry => event_from_entry(event_name_for_add(entry.kind), entry))
  }
}

///|
fn close_planned_adds(plans : Array[PlannedAdd]) -> Unit {
  for plan in plans {
    backend_close(plan.handle)
  }
}

///|
pub fn Watcher::add(
  self : Watcher,
  paths : WatchPaths,
) -> Watcher raise ChokidarError {
  let values = match paths {
    One(path) => [path]
    Many(items) => items
  }
  for path in values {
    guard path != "" else {
      raise ChokidarError("watch path must not be empty")
    }
  }
  let plans : Array[PlannedAdd] = []
  for path in values {
    let root = normalize_path(path)
    let handle = backend_watch(root, self.state.options) catch {
      ChokidarError(message) => {
        close_planned_adds(plans)
        raise ChokidarError(message)
      }
    }
    let entries = scan_snapshot(root, self.state.options) catch {
      ChokidarError(message) => {
        backend_close(handle)
        close_planned_adds(plans)
        raise ChokidarError(message)
      }
    }
    plans.push({
      root,
      handle,
      entries,
      events: initial_events_from_entries(entries, self.state.options),
    })
  }
  for plan in plans {
    self.state.add_child(path_dirname(plan.root), path_basename(plan.root))
    self.state.remember_root(plan.root)
    self.state.handles.push(plan.handle)
    self.state.set_snapshot(plan.root, plan.entries)
    for event in plan.events {
      self.queue_scanned_event(plan.root, event)
    }
  }
  self
}

///|
pub fn Watcher::unwatch(self : Watcher, paths : WatchPaths) -> Watcher {
  let values = match paths {
    One(path) => [path]
    Many(items) => items
  }
  for path in values {
    self.state.remove_child(path_dirname(path), path_basename(path))
  }
  self
}

///|
pub fn Watcher::close(self : Watcher) -> Unit {
  self.state.closed = true
  for handle in self.state.handles {
    backend_close(handle)
  }
  self.state.clear()
  self.emitter.remove_all_listeners()
}

///|
pub fn Watcher::off(
  self : Watcher,
  event : EventName,
  callback : (Event) -> Unit,
) -> Watcher {
  self.emitter.off(event, callback)
  self
}

///|
pub fn Watcher::remove_all_listeners(self : Watcher) -> Watcher {
  self.emitter.remove_all_listeners()
  self
}

///|
pub fn Watcher::get_watched(self : Watcher) -> Map[String, Array[String]] {
  self.state.get_watched()
}

///|
fn Watcher::dispatch_ready_event(self : Watcher, event : Event) -> Unit {
  let normalized = self.state.normalize_atomic(event)
  for item in normalized {
    if self.state.allow_event(item.name, item.path) {
      self.emitter.emit(item)
    }
  }
}

///|
fn Watcher::emit_event(self : Watcher, event : Event) -> Unit {
  if self.state.closed ||
    is_ignored(event.path, stats=event.stats, self.state.options.ignored) {
    return
  }
  let event = if self.state.options.always_stat &&
    event.stats is None &&
    (event.name == Add || event.name == AddDir || event.name == Change) {
    let full = absolute_path_for(event.path, self.state.options)
    match stat_for_path(full, self.state.options.follow_symlinks) {
      None => return
      Some(s) => { ..event, stats: Some(s) }
    }
  } else {
    event
  }
  let ready_events = self.state.normalize_await_write(event)
  for ready in ready_events {
    self.dispatch_ready_event(ready)
  }
}

///|
fn Watcher::record_event_in_watched(self : Watcher, event : Event) -> Unit {
  match event.name {
    Add | AddDir =>
      self.state.add_child(path_dirname(event.path), path_basename(event.path))
    Unlink | UnlinkDir =>
      self.state.remove_child(
        path_dirname(event.path),
        path_basename(event.path),
      )
    Change | Ready | Raw | Error | All => ()
  }
}

///|
fn Watcher::emit_scanned_event(
  self : Watcher,
  root : String,
  event : Event,
) -> Unit {
  let path = event_path_for(join_path(root, event.path), self.state.options)
  let event = Event::{ ..event, path, }
  self.record_event_in_watched(event)
  self.emit_event(event)
}

///|
fn Watcher::queue_scanned_event(
  self : Watcher,
  root : String,
  event : Event,
) -> Unit {
  let path = event_path_for(join_path(root, event.path), self.state.options)
  let event = Event::{ ..event, path, }
  self.record_event_in_watched(event)
  self.state.pending_events.push(event)
}

///|
fn Watcher::scan_root(
  self : Watcher,
  root : String,
  initial~ : Bool,
) -> Unit raise ChokidarError {
  let old_entries = self.state.snapshot_for(root)
  let new_entries = scan_snapshot(root, self.state.options)
  self.state.set_snapshot(root, new_entries)
  let events = if initial {
    if self.state.options.ignore_initial {
      []
    } else {
      new_entries.map(entry => {
        event_from_entry(event_name_for_add(entry.kind), entry)
      })
    }
  } else {
    diff_snapshots(old_entries, new_entries)
  }
  for event in events {
    if initial {
      self.queue_scanned_event(root, event)
    } else {
      self.emit_scanned_event(root, event)
    }
  }
}

///|
fn raw_event_affects_root(raw : RawEvent, root : String) -> Bool {
  if raw.watched_path == "" {
    true
  } else if raw.watched_path == root {
    true
  } else {
    match raw.event_path {
      Some(path) =>
        recursive_path_matches(root, join_path(raw.watched_path, path))
      None => false
    }
  }
}

///|
fn path_for_raw_event(raw : RawEvent, options : Options) -> String {
  match raw.event_path {
    Some(path) => event_path_for(join_path(raw.watched_path, path), options)
    None => event_path_for(raw.watched_path, options)
  }
}

///|
fn Watcher::process_raw_events(
  self : Watcher,
  raw_events : Array[RawEvent],
) -> Unit raise ChokidarError {
  for event in self.state.pending_events {
    self.emit_event(event)
  }
  self.state.pending_events.clear()
  if !self.state.ready {
    self.state.ready = true
    self.emit_event({
      name: Ready,
      path: "",
      stats: None,
      raw: None,
      error: None,
    })
  }
  for raw in raw_events {
    self.emit_event({
      name: Raw,
      path: path_for_raw_event(raw, self.state.options),
      stats: None,
      raw: Some(raw),
      error: None,
    })
  }
  if self.state.options.use_polling {
    for root in self.state.root_paths {
      self.scan_root(root, initial=false)
    }
  } else {
    for raw in raw_events {
      for root in self.state.root_paths {
        if raw_event_affects_root(raw, root) {
          self.scan_root(root, initial=false)
        }
      }
    }
  }
}

///|
fn Watcher::finish_process_cycle(self : Watcher) -> Unit {
  for ready in self.state.flush_stable_writes() {
    self.dispatch_ready_event(ready)
  }
  for pending in self.state.flush_expired_pending_unlinks() {
    if self.state.allow_event(pending.name, pending.path) {
      self.emitter.emit(pending)
    }
  }
  self.state.clear_throttles()
}

///|
pub fn Watcher::process(self : Watcher) -> Unit raise ChokidarError {
  if self.state.closed {
    return
  }
  let raw_events = backend_drain_events()
  let error = try {
    self.process_raw_events(raw_events)
    None
  } catch {
    ChokidarError(message) => Some(message)
  }
  self.finish_process_cycle()
  match error {
    Some(message) => {
      self.emitter.emit({
        name: Error,
        path: "",
        stats: None,
        raw: None,
        error: Some(ChokidarError(message)),
      })
      raise ChokidarError(message)
    }
    None => ()
  }
}
