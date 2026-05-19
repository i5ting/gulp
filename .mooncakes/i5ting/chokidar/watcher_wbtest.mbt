///|
test "watcher emits initial add events and ready" {
  let root = "./_build/readdirp-chokidar-watch-initial"
  backend_drain_events() |> ignore
  test_remove_tree(root)
  test_create_dir(root)
  test_write_file(join_path(root, "a.txt"), "a")
  let options = Options::{
    ..Options::default(),
    cwd: Some(root),
    use_polling: true,
  }
  let watcher = Watcher::new(options)
  let seen : Array[EventName] = []
  watcher
  .on(All, event => seen.push(event.name))
  .on(Ready, event => seen.push(event.name))
  |> ignore
  watcher.add(One(root)) |> ignore
  watcher.process()
  assert_true(seen.contains(Add))
  assert_true(seen.contains(Ready))
  watcher.close()
  test_remove_tree(root)
}

///|
test "watcher detects add change and unlink after raw pulses" {
  let root = "./_build/readdirp-chokidar-watch-diff"
  backend_drain_events() |> ignore
  test_remove_tree(root)
  test_create_dir(root)
  let options = Options::{
    ..Options::default(),
    cwd: Some(root),
    ignore_initial: true,
    use_polling: true,
    atomic_option: AtomicOff,
  }
  let watcher = Watcher::new(options)
  let seen : Array[String] = []
  watcher.on(All, event => seen.push(event.name.to_string() + ":" + event.path))
  |> ignore
  watcher.add(One(root)) |> ignore
  watcher.process()
  test_write_file(join_path(root, "a.txt"), "a")
  backend_push_event_for_test("change", root, None)
  watcher.process()
  test_sleep_millis(5)
  test_write_file(join_path(root, "a.txt"), "aa")
  backend_push_event_for_test("change", root, Some("a.txt"))
  watcher.process()
  test_remove_tree(join_path(root, "a.txt"))
  backend_push_event_for_test("change", root, Some("a.txt"))
  watcher.process()
  assert_true(seen.contains("Add:a.txt"))
  assert_true(seen.contains("Change:a.txt"))
  assert_true(seen.contains("Unlink:a.txt"))
  watcher.close()
  test_remove_tree(root)
}

///|
test "watcher emits ready only once" {
  let root = "./_build/readdirp-chokidar-watch-ready-once"
  backend_drain_events() |> ignore
  test_remove_tree(root)
  test_create_dir(root)
  let watcher = Watcher::new(Options::{
    ..Options::default(),
    use_polling: true,
  })
  let seen : Array[EventName] = []
  watcher.on(Ready, event => seen.push(event.name)) |> ignore
  watcher.add(One(root)) |> ignore
  watcher.process()
  watcher.process()
  inspect(seen, content="[Ready]")
  watcher.close()
  test_remove_tree(root)
}

///|
test "watcher close clears state and suppresses later raw pulses" {
  let root = "./_build/readdirp-chokidar-watch-close"
  backend_drain_events() |> ignore
  test_remove_tree(root)
  test_create_dir(root)
  test_write_file(join_path(root, "a.txt"), "a")
  let watcher = Watcher::new(Options::{
    ..Options::default(),
    use_polling: true,
  })
  let seen : Array[EventName] = []
  watcher.on(All, event => seen.push(event.name)) |> ignore
  watcher.add(One(root)) |> ignore
  assert_false(watcher.state.handles.is_empty())
  assert_false(watcher.state.root_paths.is_empty())
  assert_false(watcher.state.snapshots.is_empty())
  watcher.process()
  watcher.close()
  assert_true(watcher.state.closed)
  assert_true(watcher.state.handles.is_empty())
  assert_true(watcher.state.root_paths.is_empty())
  assert_true(watcher.state.snapshots.is_empty())
  inspect(watcher.get_watched(), content="{}")
  backend_push_event_for_test("change", root, Some("a.txt"))
  watcher.process()
  inspect(seen, content="[Add]")
  backend_drain_events() |> ignore
  test_remove_tree(root)
}

///|
test "watcher raw listeners observe raw events but all listeners do not" {
  let root = "./_build/readdirp-chokidar-watch-raw"
  backend_drain_events() |> ignore
  test_remove_tree(root)
  test_create_dir(root)
  let watcher = Watcher::new(Options::{
    ..Options::default(),
    ignore_initial: true,
    use_polling: true,
  })
  let raw_seen : Array[EventName] = []
  let all_seen : Array[EventName] = []
  watcher
  .on(Raw, event => raw_seen.push(event.name))
  .on(All, event => all_seen.push(event.name))
  |> ignore
  watcher.add(One(root)) |> ignore
  watcher.process()
  backend_push_event_for_test("change", root, None)
  watcher.process()
  inspect(raw_seen, content="[Raw]")
  inspect(all_seen, content="[]")
  watcher.close()
  test_remove_tree(root)
}

///|
test "await write finish respects poll interval" {
  let root = "./_build/readdirp-chokidar-watch-poll-interval"
  backend_drain_events() |> ignore
  test_remove_tree(root)
  test_create_dir(root)
  let watcher = Watcher::new(Options::{
    ..Options::default(),
    cwd: Some(root),
    ignore_initial: true,
    use_polling: true,
    await_write_finish: AwaitWriteFinishOn({
      stability_threshold: 10000,
      poll_interval: 5000,
    }),
  })
  let seen : Array[EventName] = []
  watcher.on(All, event => seen.push(event.name)) |> ignore
  watcher.add(One(root)) |> ignore
  watcher.process()
  test_write_file(join_path(root, "a.txt"), "a")
  backend_push_event_for_test("change", root, Some("a.txt"))
  watcher.process()
  // poll_interval is 5000ms — second process within that window suppresses re-check
  backend_push_event_for_test("change", root, Some("a.txt"))
  watcher.process()
  assert_true(seen.is_empty())
  watcher.close()
  test_remove_tree(root)
}

///|
test "watcher add scan failure leaves state clean" {
  let root = "./_build/readdirp-chokidar-watch-add-failure"
  backend_drain_events() |> ignore
  test_remove_tree(root)
  test_create_dir(root)
  let watcher = Watcher::new(Options::{
    ..Options::default(),
    depth: -1,
    use_polling: true,
  })
  try watcher.add(One(root)) |> ignore catch {
    ChokidarError(message) =>
      inspect(message, content="depth must be non-negative")
  } noraise {
    _ => assert_false(true)
  }
  inspect(watcher.get_watched(), content="{}")
  assert_true(watcher.state.handles.is_empty())
  assert_true(watcher.state.root_paths.is_empty())
  assert_true(watcher.state.snapshots.is_empty())
  assert_true(watcher.state.pending_events.is_empty())
  test_remove_tree(root)
}

///|
test "raw event path does not rescan unrelated watched roots" {
  let root_a = "./_build/readdirp-chokidar-watch-raw-root-a"
  let root_b = "./_build/readdirp-chokidar-watch-raw-root-b"
  backend_drain_events() |> ignore
  test_remove_tree(root_a)
  test_remove_tree(root_b)
  test_create_dir(root_a)
  test_create_dir(root_b)
  // Write to root_b before starting the watcher so kqueue does not capture
  // the write as a native event that would trigger a root_b rescan.
  test_write_file(join_path(root_b, "only-b.txt"), "b")
  let watcher = Watcher::new(Options::{
    ..Options::default(),
    ignore_initial: true,
    use_polling: false,
  })
  let seen : Array[String] = []
  watcher.on(All, event => seen.push(event.path)) |> ignore
  watcher.add(Many([root_a, root_b])) |> ignore
  watcher.process()
  // Push a raw event for root_a. root_b should not be rescanned.
  backend_push_event_for_test("change", root_a, Some("only-a.txt"))
  watcher.process()
  inspect(seen, content="[]")
  watcher.close()
  test_remove_tree(root_a)
  test_remove_tree(root_b)
}

///|
test "watcher atomic window spans process calls" {
  let root = "./_build/readdirp-chokidar-watch-atomic-window"
  backend_drain_events() |> ignore
  test_remove_tree(root)
  test_create_dir(root)
  test_write_file(join_path(root, "a.txt"), "a")
  let watcher = Watcher::new(Options::{
    ..Options::default(),
    cwd: Some(root),
    ignore_initial: true,
    use_polling: true,
    atomic_option: AtomicWindow(100),
  })
  let seen : Array[String] = []
  watcher.on(All, event => seen.push(event.name.to_string() + ":" + event.path))
  |> ignore
  watcher.add(One(root)) |> ignore
  watcher.process()
  test_remove_tree(join_path(root, "a.txt"))
  backend_push_event_for_test("change", root, Some("a.txt"))
  watcher.process()
  test_write_file(join_path(root, "a.txt"), "aa")
  backend_push_event_for_test("change", root, Some("a.txt"))
  watcher.process()
  inspect(seen, content="[\"Change:a.txt\"]")
  watcher.close()
  test_remove_tree(root)
}

///|
test "watcher atomic unlink expires after window" {
  let root = "./_build/readdirp-chokidar-watch-atomic-expire"
  backend_drain_events() |> ignore
  test_remove_tree(root)
  test_create_dir(root)
  test_write_file(join_path(root, "a.txt"), "a")
  let watcher = Watcher::new(Options::{
    ..Options::default(),
    cwd: Some(root),
    ignore_initial: true,
    use_polling: true,
    atomic_option: AtomicWindow(1),
  })
  let seen : Array[String] = []
  watcher.on(All, event => seen.push(event.name.to_string() + ":" + event.path))
  |> ignore
  watcher.add(One(root)) |> ignore
  watcher.process()
  test_remove_tree(join_path(root, "a.txt"))
  backend_push_event_for_test("change", root, Some("a.txt"))
  watcher.process()
  inspect(seen, content="[]")
  test_sleep_millis(5)
  watcher.process()
  inspect(seen, content="[\"Unlink:a.txt\"]")
  watcher.close()
  test_remove_tree(root)
}

///|
test "process emits error event before re-raising" {
  let root = "./_build/readdirp-chokidar-watch-error-event"
  backend_drain_events() |> ignore
  test_remove_tree(root)
  test_create_dir(root)
  let watcher = Watcher::new(Options::{
    ..Options::default(),
    depth: -1,
    use_polling: true,
  })
  let error_seen : Array[String] = []
  watcher.on(Error, event => {
    match event.error {
      Some(ChokidarError(message)) => error_seen.push(message)
      None => ()
    }
  })
  |> ignore
  watcher.state.remember_root(root)
  backend_push_event_for_test("change", root, None)
  try watcher.process() catch {
    ChokidarError(_) => ()
  } noraise {
    _ => assert_false(true)
  }
  assert_true(error_seen.length() > 0)
  assert_true(error_seen[0].has_prefix("depth must be non-negative"))
  watcher.close()
  test_remove_tree(root)
}

///|
test "always stat attaches stats to add and change events" {
  let root = "./_build/readdirp-chokidar-watch-always-stat"
  backend_drain_events() |> ignore
  test_remove_tree(root)
  test_create_dir(root)
  test_write_file(join_path(root, "a.txt"), "hello")
  let watcher = Watcher::new(Options::{
    ..Options::default(),
    cwd: Some(root),
    always_stat: true,
    use_polling: true,
  })
  let events : Array[Event] = []
  watcher.on(Add, event => events.push(event)) |> ignore
  watcher.add(One(root)) |> ignore
  watcher.process()
  assert_false(events.is_empty())
  assert_true(events[0].stats is Some(_))
  watcher.close()
  test_remove_tree(root)
}

///|
test "process clears throttles when scan raises" {
  let root = "./_build/readdirp-chokidar-watch-process-error"
  backend_drain_events() |> ignore
  test_remove_tree(root)
  test_create_dir(root)
  let watcher = Watcher::new(Options::{
    ..Options::default(),
    depth: -1,
    use_polling: true,
  })
  watcher.state.remember_root(root)
  watcher.state.throttled_changes.push("a.txt")
  backend_push_event_for_test("change", root, Some("a.txt"))
  try watcher.process() catch {
    ChokidarError(message) =>
      inspect(message, content="depth must be non-negative")
  } noraise {
    _ => assert_false(true)
  }
  assert_true(watcher.state.throttled_changes.is_empty())
  watcher.close()
  test_remove_tree(root)
}

///|
test "watcher off removes specific listener" {
  let root = "./_build/readdirp-chokidar-watch-off"
  backend_drain_events() |> ignore
  test_remove_tree(root)
  test_create_dir(root)
  let watcher = Watcher::new(Options::{
    ..Options::default(),
    ignore_initial: true,
    use_polling: true,
  })
  let seen : Array[EventName] = []
  let cb : (Event) -> Unit = event => seen.push(event.name)
  watcher.on(All, cb) |> ignore
  watcher.add(One(root)) |> ignore
  watcher.process()
  backend_push_event_for_test("change", root, Some("a.txt"))
  watcher.process()
  let before = seen.length()
  watcher.off(All, cb) |> ignore
  backend_push_event_for_test("change", root, Some("a.txt"))
  watcher.process()
  assert_true(seen.length() == before)
  watcher.close()
  test_remove_tree(root)
}

///|
test "watcher remove_all_listeners silences all events" {
  let root = "./_build/readdirp-chokidar-watch-remove-all"
  backend_drain_events() |> ignore
  test_remove_tree(root)
  test_create_dir(root)
  let watcher = Watcher::new(Options::{
    ..Options::default(),
    ignore_initial: true,
    use_polling: true,
  })
  let seen : Array[EventName] = []
  watcher.on(All, event => seen.push(event.name)) |> ignore
  watcher.add(One(root)) |> ignore
  watcher.process()
  watcher.remove_all_listeners() |> ignore
  backend_push_event_for_test("change", root, Some("a.txt"))
  watcher.process()
  assert_true(seen.is_empty())
  watcher.close()
  test_remove_tree(root)
}

///|
test "CHOKIDAR_USEPOLLING=true enables polling" {
  test_setenv("CHOKIDAR_USEPOLLING", "true")
  let opts = apply_env_overrides(Options::{ ..Options::default(), use_polling: false })
  test_unsetenv("CHOKIDAR_USEPOLLING")
  assert_true(opts.use_polling)
}

///|
test "CHOKIDAR_USEPOLLING=false disables polling" {
  test_setenv("CHOKIDAR_USEPOLLING", "false")
  let opts = apply_env_overrides(Options::{ ..Options::default(), use_polling: true })
  test_unsetenv("CHOKIDAR_USEPOLLING")
  assert_false(opts.use_polling)
}

///|
test "CHOKIDAR_USEPOLLING=1 enables polling" {
  test_setenv("CHOKIDAR_USEPOLLING", "1")
  let opts = apply_env_overrides(Options::{ ..Options::default(), use_polling: false })
  test_unsetenv("CHOKIDAR_USEPOLLING")
  assert_true(opts.use_polling)
}

///|
test "CHOKIDAR_INTERVAL overrides interval option" {
  test_setenv("CHOKIDAR_INTERVAL", "500")
  let opts = apply_env_overrides(Options::{ ..Options::default(), interval: 100 })
  test_unsetenv("CHOKIDAR_INTERVAL")
  inspect(opts.interval, content="500")
}

///|
test "missing env vars leave options unchanged" {
  test_unsetenv("CHOKIDAR_USEPOLLING")
  test_unsetenv("CHOKIDAR_INTERVAL")
  let base = Options::{ ..Options::default(), use_polling: true, interval: 42 }
  let opts = apply_env_overrides(base)
  assert_true(opts.use_polling)
  inspect(opts.interval, content="42")
}

///|
test "atomic mode silently drops swap-file unlink events" {
  let root = "./_build/readdirp-chokidar-watch-swapfile"
  backend_drain_events() |> ignore
  test_remove_tree(root)
  test_create_dir(root)
  test_write_file(join_path(root, ".a.txt.swp"), "swap")
  let watcher = Watcher::new(Options::{
    ..Options::default(),
    cwd: Some(root),
    ignore_initial: true,
    use_polling: true,
    atomic_option: AtomicDefault,
  })
  let seen : Array[String] = []
  watcher.on(All, event => seen.push(event.name.to_string() + ":" + event.path))
  |> ignore
  watcher.add(One(root)) |> ignore
  watcher.process()
  test_remove_tree(join_path(root, ".a.txt.swp"))
  backend_push_event_for_test("change", root, Some(".a.txt.swp"))
  watcher.process()
  test_sleep_millis(200)
  watcher.process()
  // swap-file unlink is silently dropped; no Unlink event should be emitted
  assert_true(seen.filter(s => s.has_prefix("Unlink")).is_empty())
  watcher.close()
  test_remove_tree(root)
}
