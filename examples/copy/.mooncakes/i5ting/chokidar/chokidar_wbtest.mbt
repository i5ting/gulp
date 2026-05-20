///|
fn Watcher::emit_internal_for_test(self : Watcher, event : Event) -> Unit {
  if !self.state.closed {
    self.emitter.emit(event)
  }
}

///|
test "path helpers normalize unix style paths" {
  inspect(normalize_path("a//b/"), content="a/b")
  inspect(normalize_path(""), content=".")
  inspect(join_path("a/b", "c.txt"), content="a/b/c.txt")
  inspect(path_basename("a/b/c.txt"), content="c.txt")
  inspect(path_dirname("a/b/c.txt"), content="a/b")
  inspect(relative_path("/tmp/root", "/tmp/root/a/b.txt"), content="a/b.txt")
}

///|
test "path helpers handle roots windows separators and empty joins" {
  inspect(path_dirname("/"), content="/")
  inspect(path_dirname("/a"), content="/")
  inspect(path_basename("a\\b\\c.txt"), content="c.txt")
  inspect(path_dirname("a\\b\\c.txt"), content="a/b")
  inspect(join_path("a\\b", "c.txt"), content="a/b/c.txt")
  inspect(join_path("/", ""), content="/")
  inspect(join_path(".", ""), content=".")
  inspect(join_path("", ""), content=".")
}

///|
test "cwd event paths are relative when cwd is set" {
  let options = Options::{ ..Options::default(), cwd: Some("/tmp/root") }
  inspect(event_path_for("/tmp/root/a.txt", options), content="a.txt")
  inspect(
    event_path_for("/tmp/other/a.txt", options),
    content="/tmp/other/a.txt",
  )
  inspect(
    relative_path("/tmp/root", "/tmp/root-other/a.txt"),
    content="/tmp/root-other/a.txt",
  )
  inspect(
    event_path_for("/tmp/root-other/a.txt", options),
    content="/tmp/root-other/a.txt",
  )
}

///|
test "ignored matchers support exact recursive and function forms" {
  let stats = FileStats::{ size: 1L, mode: 0, modified_nanos: 1L }
  let ignored = [
    MatchPath("skip.txt"),
    MatchRecursivePath("dist"),
    MatchFunction((path, _stats) => path.has_suffix(".tmp")),
  ]
  assert_true(is_ignored("skip.txt", stats=Some(stats), ignored))
  assert_true(is_ignored("dist/a.js", stats=Some(stats), ignored))
  assert_true(is_ignored("a.tmp", stats=Some(stats), ignored))
  assert_false(is_ignored("src/a.js", stats=Some(stats), ignored))
}

///|
test "recursive path matcher handles root and sibling prefixes" {
  assert_true(recursive_path_matches("/", "/tmp/a"))
  assert_false(recursive_path_matches("dist", "dist2/a.js"))
}

///|
test "watched directory state tracks children and removes them" {
  let state = WatchState::new(Options::default())
  state.add_child("/tmp/root", "a.txt")
  state.add_child("/tmp/root", "b.txt")
  inspect(
    state.get_watched(),
    content="{\"/tmp/root\": [\"a.txt\", \"b.txt\"]}",
  )
  state.remove_child("/tmp/root", "a.txt")
  inspect(state.get_watched(), content="{\"/tmp/root\": [\"b.txt\"]}")
}

///|
test "watched directory state removes empty directories" {
  let state = WatchState::new(Options::default())
  state.add_child("/tmp/root", "a.txt")
  inspect(state.get_watched(), content="{\"/tmp/root\": [\"a.txt\"]}")
  state.remove_child("/tmp/root", "a.txt")
  inspect(state.get_watched(), content="{}")
}

///|
test "watcher exposes chainable methods and dispatches all events" {
  let watcher = Watcher::new(Options::default())
  let seen : Array[EventName] = []
  watcher
  .on(Add, event => seen.push(event.name))
  .on(All, event => seen.push(event.name))
  |> ignore
  watcher.emit_internal_for_test(Event::{
    name: Add,
    path: "a.txt",
    stats: None,
    raw: None,
    error: None,
  })
  inspect(seen, content="[Add, Add]")
  watcher.close()
  watcher.emit_internal_for_test(Event::{
    name: Change,
    path: "a.txt",
    stats: None,
    raw: None,
    error: None,
  })
  inspect(seen, content="[Add, Add]")
}

///|
test "new listeners added during dispatch wait for later events" {
  let watcher = Watcher::new(Options::default())
  let seen : Array[String] = []
  watcher
  .on(Add, _event => {
    seen.push("add")
    watcher.on(All, _event => seen.push("late-all")) |> ignore
  })
  .on(All, _event => seen.push("early-all"))
  |> ignore
  watcher.emit_internal_for_test(Event::{
    name: Add,
    path: "a.txt",
    stats: None,
    raw: None,
    error: None,
  })
  inspect(seen, content="[\"add\", \"early-all\"]")
  watcher.emit_internal_for_test(Event::{
    name: Change,
    path: "a.txt",
    stats: None,
    raw: None,
    error: None,
  })
  inspect(seen, content="[\"add\", \"early-all\", \"early-all\", \"late-all\"]")
}

///|
test "closing during dispatch preserves snapshot and suppresses later events" {
  let watcher = Watcher::new(Options::default())
  let seen : Array[String] = []
  watcher
  .on(Add, _event => {
    seen.push("add")
    watcher.close()
  })
  .on(All, _event => seen.push("all"))
  |> ignore
  watcher.emit_internal_for_test(Event::{
    name: Add,
    path: "a.txt",
    stats: None,
    raw: None,
    error: None,
  })
  inspect(seen, content="[\"add\", \"all\"]")
  watcher.emit_internal_for_test(Event::{
    name: Change,
    path: "a.txt",
    stats: None,
    raw: None,
    error: None,
  })
  inspect(seen, content="[\"add\", \"all\"]")
}

///|
test "direct all events notify all listeners once" {
  let watcher = Watcher::new(Options::default())
  let seen : Array[EventName] = []
  watcher.on(All, event => seen.push(event.name)) |> ignore
  watcher.emit_internal_for_test(Event::{
    name: All,
    path: "a.txt",
    stats: None,
    raw: None,
    error: None,
  })
  inspect(seen, content="[All]")
}

///|
test "atomic converts unlink plus add into change" {
  let state = WatchState::new(Options::{
    ..Options::default(),
    atomic_option: AtomicWindow(100),
  })
  let unlink = Event::{
    name: Unlink,
    path: "a.txt",
    stats: None,
    raw: None,
    error: None,
  }
  let add = Event::{
    name: Add,
    path: "a.txt",
    stats: Some({ size: 2L, mode: 0, modified_nanos: 2L }),
    raw: None,
    error: None,
  }
  let first = state.normalize_atomic(unlink)
  inspect(first.length(), content="0")
  let second = state.normalize_atomic(add)
  inspect(second.length(), content="1")
  assert_true(second[0].name == Change)
}

///|
test "change throttle suppresses duplicate changes in same process pass" {
  let state = WatchState::new(Options::default())
  assert_true(state.allow_event(Change, "a.txt"))
  assert_false(state.allow_event(Change, "a.txt"))
  state.clear_throttles()
  assert_true(state.allow_event(Change, "a.txt"))
}

///|
enum CompatibilityStatus {
  Same
  RenamedForMoonBitStyle
  PlatformDependent
} derive(Eq)

///|
struct CompatibilityRow {
  js_name : String
  moonbit_name : String
  status : CompatibilityStatus
}

///|
fn compatibility_table() -> Array[CompatibilityRow] {
  [
    { js_name: "watch", moonbit_name: "watch", status: Same },
    { js_name: "FSWatcher.add", moonbit_name: "Watcher::add", status: Same },
    { js_name: "FSWatcher.on", moonbit_name: "Watcher::on", status: Same },
    {
      js_name: "FSWatcher.unwatch",
      moonbit_name: "Watcher::unwatch",
      status: Same,
    },
    { js_name: "FSWatcher.off", moonbit_name: "Watcher::off", status: Same },
    {
      js_name: "FSWatcher.removeAllListeners",
      moonbit_name: "Watcher::remove_all_listeners",
      status: RenamedForMoonBitStyle,
    },
    {
      js_name: "FSWatcher.close",
      moonbit_name: "Watcher::close",
      status: RenamedForMoonBitStyle,
    },
    {
      js_name: "FSWatcher.getWatched",
      moonbit_name: "Watcher::get_watched",
      status: RenamedForMoonBitStyle,
    },
    {
      js_name: "ignoreInitial",
      moonbit_name: "ignore_initial",
      status: RenamedForMoonBitStyle,
    },
    {
      js_name: "followSymlinks",
      moonbit_name: "follow_symlinks",
      status: RenamedForMoonBitStyle,
    },
    {
      js_name: "usePolling",
      moonbit_name: "use_polling",
      status: RenamedForMoonBitStyle,
    },
    {
      js_name: "binaryInterval",
      moonbit_name: "binary_interval",
      status: RenamedForMoonBitStyle,
    },
    {
      js_name: "alwaysStat",
      moonbit_name: "always_stat",
      status: RenamedForMoonBitStyle,
    },
    {
      js_name: "ignorePermissionErrors",
      moonbit_name: "ignore_permission_errors",
      status: RenamedForMoonBitStyle,
    },
    {
      js_name: "awaitWriteFinish",
      moonbit_name: "await_write_finish",
      status: RenamedForMoonBitStyle,
    },
    {
      js_name: "atomic",
      moonbit_name: "atomic_option",
      status: RenamedForMoonBitStyle,
    },
    {
      js_name: "RegExp ignored matcher",
      moonbit_name: "MatchRegex(@regexp.Regexp)",
      status: Same,
    },
    {
      js_name: "raw event details",
      moonbit_name: "RawEvent",
      status: PlatformDependent,
    },
  ]
}

///|
test "compatibility table records MoonBit naming differences" {
  let rows = compatibility_table()
  inspect(rows.length(), content="18")
  assert_true(rows.any(row => row.js_name == "watch" && row.status == Same))
  assert_true(
    rows.any(row => {
      row.js_name == "FSWatcher.add" &&
      row.moonbit_name == "Watcher::add" &&
      row.status == Same
    }),
  )
  assert_true(
    rows.any(row => {
      row.js_name == "FSWatcher.on" &&
      row.moonbit_name == "Watcher::on" &&
      row.status == Same
    }),
  )
  assert_true(
    rows.any(row => {
      row.js_name == "FSWatcher.unwatch" &&
      row.moonbit_name == "Watcher::unwatch" &&
      row.status == Same
    }),
  )
  assert_true(
    rows.any(row => {
      row.js_name == "FSWatcher.close" &&
      row.moonbit_name == "Watcher::close" &&
      row.status == RenamedForMoonBitStyle
    }),
  )
  assert_true(
    rows.any(row => {
      row.js_name == "FSWatcher.getWatched" &&
      row.moonbit_name == "Watcher::get_watched" &&
      row.status == RenamedForMoonBitStyle
    }),
  )
  assert_true(
    rows.any(row => {
      row.js_name == "ignoreInitial" &&
      row.moonbit_name == "ignore_initial" &&
      row.status == RenamedForMoonBitStyle
    }),
  )
  assert_true(
    rows.any(row => {
      row.js_name == "followSymlinks" &&
      row.moonbit_name == "follow_symlinks" &&
      row.status == RenamedForMoonBitStyle
    }),
  )
  assert_true(
    rows.any(row => {
      row.js_name == "usePolling" &&
      row.moonbit_name == "use_polling" &&
      row.status == RenamedForMoonBitStyle
    }),
  )
  assert_true(
    rows.any(row => {
      row.js_name == "binaryInterval" &&
      row.moonbit_name == "binary_interval" &&
      row.status == RenamedForMoonBitStyle
    }),
  )
  assert_true(
    rows.any(row => {
      row.js_name == "alwaysStat" &&
      row.moonbit_name == "always_stat" &&
      row.status == RenamedForMoonBitStyle
    }),
  )
  assert_true(
    rows.any(row => {
      row.js_name == "ignorePermissionErrors" &&
      row.moonbit_name == "ignore_permission_errors" &&
      row.status == RenamedForMoonBitStyle
    }),
  )
  assert_true(
    rows.any(row => {
      row.js_name == "awaitWriteFinish" &&
      row.moonbit_name == "await_write_finish" &&
      row.status == RenamedForMoonBitStyle
    }),
  )
  assert_true(
    rows.any(row => {
      row.js_name == "atomic" &&
      row.moonbit_name == "atomic_option" &&
      row.status == RenamedForMoonBitStyle
    }),
  )
  assert_true(
    rows.any(row => {
      row.js_name == "RegExp ignored matcher" && row.status == Same
    }),
  )
  assert_true(
    rows.any(row => {
      row.js_name == "raw event details" &&
      row.moonbit_name == "RawEvent" &&
      row.status == PlatformDependent
    }),
  )
}

///|
test "await write finish suppresses add events without stats" {
  let watcher = Watcher::new(Options::{
    ..Options::default(),
    await_write_finish: AwaitWriteFinishOn({
      stability_threshold: 1,
      poll_interval: 1,
    }),
  })
  let seen : Array[EventName] = []
  watcher.on(All, event => seen.push(event.name)) |> ignore
  watcher.emit_event(Event::{
    name: Add,
    path: "a.txt",
    stats: None,
    raw: None,
    error: None,
  })
  inspect(seen, content="[]")
}
