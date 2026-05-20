///|
priv struct DirEntry {
  path : String
  mut children : Array[String]
}

///|
priv struct PendingTimedEvent {
  event : Event
  created_at : UInt64
}

///|
priv struct PendingWrite {
  mut event : Event
  mut stats : FileStats
  mut stable_since : UInt64
  mut last_checked : UInt64
}

///|
priv struct WatchState {
  options : Options
  dirs : Array[DirEntry]
  root_paths : Array[String]
  snapshots : Map[String, Array[SnapshotEntry]]
  handles : Array[BackendHandle]
  pending_events : Array[Event]
  pending_unlinks : Array[PendingTimedEvent]
  pending_writes : Array[PendingWrite]
  throttled_changes : Array[String]
  mut closed : Bool
  mut ready : Bool
}

///|
fn WatchState::new(options : Options) -> WatchState {
  {
    options,
    dirs: [],
    root_paths: [],
    snapshots: Map::new(),
    handles: [],
    pending_events: [],
    pending_unlinks: [],
    pending_writes: [],
    throttled_changes: [],
    closed: false,
    ready: false,
  }
}

///|
fn WatchState::dir_index(self : WatchState, dir : String) -> Int? {
  for index, entry in self.dirs {
    if entry.path == dir {
      break Some(index)
    }
  } nobreak {
    None
  }
}

///|
fn WatchState::ensure_dir(self : WatchState, dir : String) -> Int {
  match self.dir_index(dir) {
    Some(index) => index
    None => {
      self.dirs.push({ path: dir, children: [] })
      self.dirs.length() - 1
    }
  }
}

///|
fn WatchState::add_child(
  self : WatchState,
  dir : String,
  child : String,
) -> Unit {
  let index = self.ensure_dir(dir)
  let entry = self.dirs[index]
  if !entry.children.contains(child) {
    entry.children.push(child)
    entry.children.sort()
  }
}

///|
fn WatchState::remove_child(
  self : WatchState,
  dir : String,
  child : String,
) -> Unit {
  match self.dir_index(dir) {
    Some(index) => {
      let entry = self.dirs[index]
      entry.children = entry.children.filter(item => item != child)
      if entry.children.is_empty() {
        self.dirs.remove(index) |> ignore
      }
    }
    None => ()
  }
}

///|
fn WatchState::get_watched(self : WatchState) -> Map[String, Array[String]] {
  let watched = Map::new()
  for entry in self.dirs {
    watched[entry.path] = entry.children.copy()
  }
  watched
}

///|
fn WatchState::remember_root(self : WatchState, root : String) -> Unit {
  if !self.root_paths.contains(root) {
    self.root_paths.push(root)
  }
}

///|
fn WatchState::set_snapshot(
  self : WatchState,
  root : String,
  entries : Array[SnapshotEntry],
) -> Unit {
  self.snapshots[root] = entries
}

///|
fn WatchState::snapshot_for(
  self : WatchState,
  root : String,
) -> Array[SnapshotEntry] {
  match self.snapshots.get(root) {
    Some(entries) => entries
    None => []
  }
}

///|
fn WatchState::clear(self : WatchState) -> Unit {
  self.dirs.clear()
  self.root_paths.clear()
  self.snapshots.clear()
  self.handles.clear()
  self.pending_events.clear()
  self.pending_unlinks.clear()
  self.pending_writes.clear()
  self.throttled_changes.clear()
}

///|
fn WatchState::pending_unlink_index(self : WatchState, path : String) -> Int? {
  for index, pending in self.pending_unlinks {
    if pending.event.path == path {
      break Some(index)
    }
  } nobreak {
    None
  }
}

///|
fn millis_to_uint64(millis : Int) -> UInt64 {
  if millis <= 0 {
    (0).to_uint64()
  } else {
    millis.reinterpret_as_uint().to_uint64()
  }
}

///|
fn atomic_window_millis(option : AtomicOption) -> Int {
  match option {
    AtomicOff => 0
    AtomicDefault => 100
    AtomicWindow(window) => window
  }
}

///|
fn await_write_threshold_millis(option : AwaitWriteFinish) -> Int {
  match option {
    AwaitWriteFinishOff => 0
    AwaitWriteFinishOn(config) => config.stability_threshold
  }
}

///|
fn timestamp_expired(started_at : UInt64, window_millis : Int) -> Bool {
  @env.now() >= started_at + millis_to_uint64(window_millis)
}

///|
fn WatchState::normalize_atomic(
  self : WatchState,
  event : Event,
) -> Array[Event] {
  match self.options.atomic_option {
    AtomicOff => [event]
    AtomicDefault | AtomicWindow(_) =>
      if event.name == Unlink {
        if is_dot_swap_file(event.path) {
          return []
        }
        self.pending_unlinks.push({ event, created_at: @env.now() })
        []
      } else if event.name == Add {
        match self.pending_unlink_index(event.path) {
          Some(index) => {
            let pending = self.pending_unlinks.remove(index)
            if timestamp_expired(
                pending.created_at,
                atomic_window_millis(self.options.atomic_option),
              ) {
              [pending.event, event]
            } else {
              [Event::{ ..event, name: Change }]
            }
          }
          None => [event]
        }
      } else {
        [event]
      }
  }
}

///|
fn WatchState::flush_expired_pending_unlinks(self : WatchState) -> Array[Event] {
  let events : Array[Event] = []
  let window = atomic_window_millis(self.options.atomic_option)
  let mut index = 0
  while index < self.pending_unlinks.length() {
    let pending = self.pending_unlinks[index]
    if timestamp_expired(pending.created_at, window) {
      events.push(pending.event)
      self.pending_unlinks.remove(index) |> ignore
    } else {
      index += 1
    }
  }
  events
}

///|
fn WatchState::allow_event(
  self : WatchState,
  name : EventName,
  path : String,
) -> Bool {
  if name != Change {
    true
  } else if self.throttled_changes.contains(path) {
    false
  } else {
    self.throttled_changes.push(path)
    true
  }
}

///|
fn WatchState::clear_throttles(self : WatchState) -> Unit {
  self.throttled_changes.clear()
}

///|
fn WatchState::pending_write_index(self : WatchState, path : String) -> Int? {
  for index, pending in self.pending_writes {
    if pending.event.path == path {
      break Some(index)
    }
  } nobreak {
    None
  }
}

///|
fn event_needs_write_stability(event : Event) -> Bool {
  event.name == Add || event.name == Change
}

///|
fn WatchState::normalize_await_write(
  self : WatchState,
  event : Event,
) -> Array[Event] {
  match self.options.await_write_finish {
    AwaitWriteFinishOff => [event]
    AwaitWriteFinishOn(_) =>
      if !event_needs_write_stability(event) {
        [event]
      } else {
        match event.stats {
          None => []
          Some(stats) => {
            let now = @env.now()
            match self.pending_write_index(event.path) {
              Some(index) => {
                let pending = self.pending_writes[index]
                let poll_interval = match self.options.await_write_finish {
                  AwaitWriteFinishOn(config) => config.poll_interval
                  AwaitWriteFinishOff => 0
                }
                if !timestamp_expired(pending.last_checked, poll_interval) {
                  []
                } else {
                  pending.last_checked = now
                  pending.event = event
                  if pending.stats == stats {
                    if timestamp_expired(
                        pending.stable_since,
                        await_write_threshold_millis(
                          self.options.await_write_finish,
                        ),
                      ) {
                      self.pending_writes.remove(index) |> ignore
                      [event]
                    } else {
                      []
                    }
                  } else {
                    pending.stats = stats
                    pending.stable_since = now
                    []
                  }
                }
              }
              None => {
                self.pending_writes.push({
                  event,
                  stats,
                  stable_since: now,
                  last_checked: now,
                })
                []
              }
            }
          }
        }
      }
  }
}

///|
fn WatchState::flush_stable_writes(self : WatchState) -> Array[Event] {
  let events : Array[Event] = []
  let threshold = await_write_threshold_millis(self.options.await_write_finish)
  let mut index = 0
  while index < self.pending_writes.length() {
    let pending = self.pending_writes[index]
    if timestamp_expired(pending.stable_since, threshold) {
      events.push(pending.event)
      self.pending_writes.remove(index) |> ignore
    } else {
      index += 1
    }
  }
  events
}
