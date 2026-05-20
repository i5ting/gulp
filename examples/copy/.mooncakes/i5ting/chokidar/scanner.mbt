///|
priv struct SnapshotEntry {
  path : String
  kind : FileKind
  stats : FileStats?
}

///|
fn file_kind_from_readdirp(kind : @readdirp.FileKind) -> FileKind {
  match kind {
    @readdirp.File => File
    @readdirp.Directory => Directory
    @readdirp.Symlink => Symlink
    @readdirp.Other => Other
  }
}

///|
#borrow(path)
extern "C" fn native_modified_nanos(
  path : Bytes,
  follow_symlink : Int,
) -> Int64 = "chokidar_scanner_modified_nanos"

///|
#borrow(path)
extern "C" fn native_stat_valid(
  path : Bytes,
  follow_symlink : Int,
) -> Int = "chokidar_scanner_stat_valid"

///|
#borrow(path)
extern "C" fn native_stat_size(
  path : Bytes,
  follow_symlink : Int,
) -> Int64 = "chokidar_scanner_stat_size"

///|
#borrow(path)
extern "C" fn native_stat_mode(
  path : Bytes,
  follow_symlink : Int,
) -> Int = "chokidar_scanner_stat_mode"

///|
fn scanner_path_to_bytes(path : String) -> Bytes {
  scanner_string_to_utf8_bytes(path)
}

///|
fn scanner_string_to_utf8_bytes(value : String) -> Bytes {
  let byte_length = value
    .iter()
    .fold(init=0, (length, char) => {
      let code = char.to_int()
      if code < 0x80 {
        length + 1
      } else if code < 0x800 {
        length + 2
      } else if code < 0x10000 {
        length + 3
      } else {
        length + 4
      }
    })
  let bytes = FixedArray::make(byte_length, b'\x00')
  ignore(
    value
    .iter()
    .fold(init=0, (offset, char) => offset + bytes.set_utf8_char(offset, char)),
  )
  scanner_fixed_byte_array_to_bytes(bytes)
}

///|
fn scanner_fixed_byte_array_to_bytes(bytes : FixedArray[Byte]) -> Bytes = "%identity"

///|
fn modified_nanos_for(path : String, follow_symlink : Bool) -> Int64 {
  native_modified_nanos(
    scanner_path_to_bytes(path),
    if follow_symlink {
      1
    } else {
      0
    },
  )
}

///|
fn stats_from_readdirp(
  entry : @readdirp.EntryInfo,
  follow_symlink : Bool,
) -> FileStats? {
  match entry.stats {
    Some(value) =>
      Some({
        size: value.size,
        mode: value.mode,
        modified_nanos: modified_nanos_for(entry.full_path, follow_symlink),
      })
    None => None
  }
}

///|
fn readdirp_type() -> @readdirp.EntryType {
  @readdirp.FilesAndDirectories
}

///|
fn warnings_to_error(
  warnings : Array[@readdirp.ReaddirpWarning],
  options : Options,
) -> ChokidarError? {
  for warning in warnings {
    if warning.kind != @readdirp.RecursiveSymlink &&
      !(options.ignore_permission_errors &&
        warning.kind == @readdirp.PermissionDenied) {
      break Some(
        ChokidarError(
          "scan warning for " + warning.path + ": " + warning.message,
        ),
      )
    }
  } nobreak {
    None
  }
}

///|
fn compare_paths(left : String, right : String) -> Int {
  let left_bytes = scanner_string_to_utf8_bytes(left)
  let right_bytes = scanner_string_to_utf8_bytes(right)
  let limit = left_bytes.length().min(right_bytes.length())
  for index in 0..<limit {
    let left_byte = left_bytes.unsafe_get(index).to_int()
    let right_byte = right_bytes.unsafe_get(index).to_int()
    if left_byte < right_byte {
      break -1
    } else if left_byte > right_byte {
      break 1
    }
  } nobreak {
    if left_bytes.length() < right_bytes.length() {
      -1
    } else if left_bytes.length() > right_bytes.length() {
      1
    } else {
      0
    }
  }
}

///|
fn compare_snapshot_paths(left : SnapshotEntry, right : SnapshotEntry) -> Int {
  compare_paths(left.path, right.path)
}

///|
#warnings("-unused_value")
fn scan_snapshot(
  root : String,
  options : Options,
) -> Array[SnapshotEntry] raise ChokidarError {
  let readdirp_options = @readdirp.Options::{
    type_: readdirp_type(),
    always_stat: true,
    lstat: !options.follow_symlinks,
    depth: options.depth,
    file_filter: entry => {
      let stats = stats_from_readdirp(entry, options.follow_symlinks)
      let readable = options.ignore_permission_errors ||
        stats is None ||
        (stats is Some(s) && (s.mode & 0x100) != 0)
      let matched_path = event_path_for(join_path(root, entry.path), options)
      readable &&
      !is_ignored(matched_path, stats~, options.ignored)
    },
    directory_filter: entry => {
      let matched_path = event_path_for(join_path(root, entry.path), options)
      !is_ignored(
        matched_path,
        stats=stats_from_readdirp(entry, options.follow_symlinks),
        options.ignored,
      )
    },
  }
  let report = @readdirp.readdirp_report(root, options=readdirp_options) catch {
    @readdirp.ReaddirpError(message) => raise ChokidarError(message)
  }
  match warnings_to_error(report.warnings, options) {
    Some(error) => raise error
    None => ()
  }
  let snapshot = report.entries.map(entry => SnapshotEntry::{
    path: entry.path,
    kind: file_kind_from_readdirp(entry.kind),
    stats: stats_from_readdirp(entry, options.follow_symlinks),
  })
  snapshot.sort_by(compare_snapshot_paths)
  snapshot
}

///|
fn snapshot_find(
  entries : Array[SnapshotEntry],
  path : String,
) -> SnapshotEntry? {
  for entry in entries {
    if entry.path == path {
      break Some(entry)
    }
  } nobreak {
    None
  }
}

///|
fn stats_changed(old_stats : FileStats?, new_stats : FileStats?) -> Bool {
  match (old_stats, new_stats) {
    (Some(old), Some(new)) =>
      old.size != new.size ||
      old.mode != new.mode ||
      old.modified_nanos != new.modified_nanos
    (None, Some(_)) => true
    (Some(_), None) => true
    (None, None) => false
  }
}

///|
fn event_name_for_add(kind : FileKind) -> EventName {
  match kind {
    Directory => AddDir
    File | Symlink | Other => Add
  }
}

///|
fn event_name_for_unlink(kind : FileKind) -> EventName {
  match kind {
    Directory => UnlinkDir
    File | Symlink | Other => Unlink
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
#warnings("-unused_value")
fn diff_snapshots(
  old_entries : Array[SnapshotEntry],
  new_entries : Array[SnapshotEntry],
) -> Array[Event] {
  let events : Array[Event] = []
  let old_entries = old_entries.copy()
  let new_entries = new_entries.copy()
  old_entries.sort_by(compare_snapshot_paths)
  new_entries.sort_by(compare_snapshot_paths)
  for new_entry in new_entries {
    match snapshot_find(old_entries, new_entry.path) {
      Some(old_entry) =>
        if new_entry.kind != old_entry.kind {
          events.push(
            event_from_entry(event_name_for_unlink(old_entry.kind), old_entry),
          )
          events.push(
            event_from_entry(event_name_for_add(new_entry.kind), new_entry),
          )
        } else if new_entry.kind != Directory &&
          stats_changed(old_entry.stats, new_entry.stats) {
          events.push(event_from_entry(Change, new_entry))
        }
      None =>
        events.push(
          event_from_entry(event_name_for_add(new_entry.kind), new_entry),
        )
    }
  }
  for old_entry in old_entries {
    match snapshot_find(new_entries, old_entry.path) {
      Some(_) => ()
      None =>
        events.push(
          event_from_entry(event_name_for_unlink(old_entry.kind), old_entry),
        )
    }
  }
  events
}

///|
fn stat_for_path(path : String, follow_symlink : Bool) -> FileStats? {
  let follow = if follow_symlink { 1 } else { 0 }
  let bytes = scanner_path_to_bytes(path)
  if native_stat_valid(bytes, follow) == 0 {
    None
  } else {
    Some({
      size: native_stat_size(bytes, follow),
      mode: native_stat_mode(bytes, follow),
      modified_nanos: native_modified_nanos(bytes, follow),
    })
  }
}
