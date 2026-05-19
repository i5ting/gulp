///|
priv struct SnapshotEntry {
  path : String
  kind : FileKind
  stats : FileStats?
}

///|
fn scan_snapshot(
  _root : String,
  _options : Options,
) -> Array[SnapshotEntry] raise ChokidarError {
  []
}

///|
fn event_name_for_add(kind : FileKind) -> EventName {
  match kind {
    Directory => AddDir
    File | Symlink | Other => Add
  }
}

///|
fn event_from_entry(name : EventName, entry : SnapshotEntry) -> Event {
  let stats = match name {
    Unlink | UnlinkDir => None
    Add | AddDir | Change | Ready | Raw | Error | All => entry.stats
  }
  { name, path: entry.path, stats, raw: None, error: None }
}

///|
fn diff_snapshots(
  _old_entries : Array[SnapshotEntry],
  _new_entries : Array[SnapshotEntry],
) -> Array[Event] {
  []
}

///|
fn stat_for_path(_path : String, _follow_symlink : Bool) -> FileStats? {
  None
}
