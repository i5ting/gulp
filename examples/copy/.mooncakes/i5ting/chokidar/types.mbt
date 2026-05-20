///|
pub(all) suberror ChokidarError {
  ChokidarError(String)
} derive(Debug, Eq)

///|
pub(all) enum WatchPaths {
  One(String)
  Many(Array[String])
} derive(Eq, Debug)

///|
pub(all) enum EventName {
  Add
  Change
  AddDir
  Unlink
  UnlinkDir
  Ready
  Raw
  Error
  All
} derive(Eq, Debug)

///|
pub impl Show for EventName with to_string(self) {
  match self {
    Add => "Add"
    Change => "Change"
    AddDir => "AddDir"
    Unlink => "Unlink"
    UnlinkDir => "UnlinkDir"
    Ready => "Ready"
    Raw => "Raw"
    Error => "Error"
    All => "All"
  }
}

///|
pub(all) enum FileKind {
  File
  Directory
  Symlink
  Other
} derive(Eq, Debug)

///|
pub(all) struct FileStats {
  size : Int64
  mode : Int
  modified_nanos : Int64
} derive(Eq, Debug)

///|
pub(all) struct RawEvent {
  backend_event_name : String
  watched_path : String
  event_path : String?
  platform : String
  error_message : String?
} derive(Eq, Debug)

///|
pub(all) struct Event {
  name : EventName
  path : String
  stats : FileStats?
  raw : RawEvent?
  error : ChokidarError?
} derive(Eq, Debug)

///|
pub(all) enum AtomicOption {
  AtomicDefault
  AtomicOff
  AtomicWindow(Int)
} derive(Eq, Debug)

///|
pub(all) struct AwaitWriteFinishOptions {
  stability_threshold : Int
  poll_interval : Int
} derive(Eq, Debug)

///|
pub(all) enum AwaitWriteFinish {
  AwaitWriteFinishOff
  AwaitWriteFinishOn(AwaitWriteFinishOptions)
} derive(Eq, Debug)

///|
pub(all) enum Matcher {
  MatchPath(String)
  MatchRecursivePath(String)
  MatchFunction((String, FileStats?) -> Bool)
  MatchRegex(@regexp.Regexp)
}

///|
pub(all) struct Options {
  persistent : Bool
  ignored : Array[Matcher]
  ignore_initial : Bool
  follow_symlinks : Bool
  cwd : String?
  use_polling : Bool
  interval : Int
  binary_interval : Int
  always_stat : Bool
  depth : Int
  ignore_permission_errors : Bool
  atomic_option : AtomicOption
  await_write_finish : AwaitWriteFinish
}

///|
pub fn Options::default() -> Options {
  {
    persistent: true,
    ignored: [],
    ignore_initial: false,
    follow_symlinks: true,
    cwd: None,
    use_polling: false,
    interval: 100,
    binary_interval: 300,
    always_stat: false,
    depth: 2147483647,
    ignore_permission_errors: false,
    atomic_option: AtomicDefault,
    await_write_finish: AwaitWriteFinishOff,
  }
}

///|
pub fn AwaitWriteFinish::default_on() -> AwaitWriteFinish {
  AwaitWriteFinishOn({ stability_threshold: 2000, poll_interval: 100 })
}

///|
pub fn all_event_names() -> Array[EventName] {
  [Add, Change, AddDir, Unlink, UnlinkDir, Ready, Raw, Error, All]
}
