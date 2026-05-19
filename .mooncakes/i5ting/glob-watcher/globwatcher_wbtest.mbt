// ── Test helpers – C stubs ────────────────────────────────────────────────────

///|
#borrow(path)
extern "C" fn gw_raw_mkdir(path : Bytes) -> Int = "globwatcher_test_mkdir"

///|
#borrow(path, contents)
extern "C" fn gw_raw_write_file(path : Bytes, contents : Bytes) -> Int = "globwatcher_test_write_file"

///|
#borrow(path)
extern "C" fn gw_raw_remove_tree(path : Bytes) -> Int = "globwatcher_test_remove_tree"

// ── String → UTF-8 Bytes ──────────────────────────────────────────────────────

fn wb_string_to_bytes(value : String) -> Bytes {
  let byte_length = value.iter().fold(
    init=0,
    (length, char) => {
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
    },
  )
  let bytes : FixedArray[Byte] = FixedArray::make(byte_length, b'\x00')
  ignore(
    value.iter().fold(
      init=0,
      (offset, char) => offset + bytes.set_utf8_char(offset, char),
    ),
  )
  wb_fixed_to_bytes(bytes)
}

fn wb_fixed_to_bytes(bytes : FixedArray[Byte]) -> Bytes = "%identity"

fn wb_b(s : String) -> Bytes {
  wb_string_to_bytes(s)
}

// ── Filesystem helpers ────────────────────────────────────────────────────────

fn wb_mkdir(path : String) -> Unit raise GlobWatcherError {
  guard gw_raw_mkdir(wb_b(path)) == 0 else {
    raise GlobError("mkdir failed: " + path)
  }
}

fn wb_write(path : String, contents : String) -> Unit raise GlobWatcherError {
  guard gw_raw_write_file(wb_b(path), wb_b(contents)) == 0 else {
    raise GlobError("write_file failed: " + path)
  }
}

fn wb_rmtree(path : String) -> Unit {
  ignore(gw_raw_remove_tree(wb_b(path)))
}

fn wb_event(name : @chokidar.EventName, path : String) -> @chokidar.Event {
  { name, path, stats: None, raw: None, error: None }
}

// ── GlobPattern unit tests ────────────────────────────────────────────────────

///|
test "GlobPattern parse basic positive glob" {
  let pats = GlobPattern::parse("src/**/*.js")
  assert_eq(pats.length(), 1)
  assert_eq(pats[0].raw, "src/**/*.js")
  assert_eq(pats[0].base_dir, "src")
  assert_false(pats[0].is_negation)
}

///|
test "GlobPattern parse negation strips prefix" {
  let pats = GlobPattern::parse("!src/**/*.js")
  assert_eq(pats.length(), 1)
  assert_eq(pats[0].raw, "src/**/*.js")
  assert_true(pats[0].is_negation)
}

///|
test "GlobPattern parse brace expansion yields multiple patterns" {
  let pats = GlobPattern::parse("src/**/*.{js,ts}")
  assert_eq(pats.length(), 2)
  assert_eq(pats[0].raw, "src/**/*.js")
  assert_eq(pats[1].raw, "src/**/*.ts")
}

///|
test "GlobPattern parse no special chars dot base_dir" {
  let pats = GlobPattern::parse("*.js")
  assert_eq(pats[0].base_dir, ".")
}

///|
test "GlobPattern parse literal path extracts directory" {
  let pats = GlobPattern::parse("src/lib/util.js")
  assert_eq(pats[0].base_dir, "src/lib")
  assert_false(pats[0].is_negation)
}

///|
test "parse_globs separates positives from negations" {
  let (pos, neg) = parse_globs(["**/*.js", "!vendor/**"])
  assert_eq(pos.length(), 1)
  assert_eq(neg.length(), 1)
  assert_eq(pos[0].raw, "**/*.js")
  assert_eq(neg[0].raw, "vendor/**")
}

///|
test "parse_globs re-add resolution cancels matching negation" {
  let (pos, neg) = parse_globs(["**/*.js", "!src/index.js", "src/index.js"])
  assert_eq(pos.length(), 2)
  assert_true(neg.is_empty())
}

///|
test "parse_globs empty input returns empty arrays" {
  let (pos, neg) = parse_globs([])
  assert_true(pos.is_empty())
  assert_true(neg.is_empty())
}

///|
test "parse_globs only negations gives empty positives" {
  let (pos, neg) = parse_globs(["!vendor/**"])
  assert_true(pos.is_empty())
  assert_eq(neg.length(), 1)
}

// ── glob_match unit tests ─────────────────────────────────────────────────────

///|
test "glob_match literal path matches exactly" {
  assert_true(glob_match("src/index.js", "src/index.js"))
  assert_false(glob_match("src/index.js", "src/index.ts"))
}

///|
test "glob_match single star within segment" {
  assert_true(glob_match("src/*.js", "src/index.js"))
  assert_false(glob_match("src/*.js", "src/lib/index.js"))
}

///|
test "glob_match double star across segments" {
  assert_true(glob_match("src/**/*.js", "src/lib/util/index.js"))
  assert_true(glob_match("**/*.js", "a/b/c/d.js"))
  assert_true(glob_match("**/*.js", "d.js"))
}

///|
test "glob_match question mark single non-separator char" {
  assert_true(glob_match("src/?.js", "src/a.js"))
  assert_false(glob_match("src/?.js", "src/ab.js"))
  assert_false(glob_match("src/?.js", "src/.js"))
}

///|
test "glob_match character class" {
  assert_true(glob_match("[abc].js", "a.js"))
  assert_true(glob_match("[abc].js", "b.js"))
  assert_false(glob_match("[abc].js", "d.js"))
}

///|
test "glob_match negated character class" {
  assert_true(glob_match("[^abc].js", "d.js"))
  assert_false(glob_match("[^abc].js", "a.js"))
}

///|
test "glob_match character range" {
  assert_true(glob_match("[a-z].js", "m.js"))
  assert_false(glob_match("[a-z].js", "A.js"))
}

///|
test "glob_match brace expansion" {
  assert_true(glob_match("*.{js,ts}", "index.js"))
  assert_true(glob_match("*.{js,ts}", "index.ts"))
  assert_false(glob_match("*.{js,ts}", "index.py"))
}

///|
test "glob_match double star at root" {
  assert_true(glob_match("**", "a/b/c"))
  assert_true(glob_match("**", "a"))
}

// ── Debouncer unit tests ──────────────────────────────────────────────────────

///|
test "Debouncer new has no pending event" {
  let d = Debouncer::new(100)
  assert_false(d.is_pending())
  assert_true(d.flush(9999UL) is None)
}

///|
test "Debouncer feed makes event pending" {
  let d = Debouncer::new(100)
  d.feed(wb_event(@chokidar.EventName::Change, "a.js"), 1000UL)
  assert_true(d.is_pending())
}

///|
test "Debouncer flush before delay returns None" {
  let d = Debouncer::new(100)
  d.feed(wb_event(@chokidar.EventName::Change, "a.js"), 1000UL)
  assert_true(d.flush(1050UL) is None)
}

///|
test "Debouncer flush at exactly delay boundary returns event" {
  let d = Debouncer::new(100)
  let e = wb_event(@chokidar.EventName::Change, "a.js")
  d.feed(e, 1000UL)
  let result = d.flush(1100UL)
  assert_true(result is Some(_))
  assert_false(d.is_pending())
}

///|
test "Debouncer subsequent feed resets the timer" {
  let d = Debouncer::new(100)
  d.feed(wb_event(@chokidar.EventName::Change, "a.js"), 1000UL)
  d.feed(wb_event(@chokidar.EventName::Change, "b.js"), 1050UL)
  // 50ms since last feed → still within debounce window
  assert_true(d.flush(1100UL) is None)
  // 100ms since last feed → flush with the LAST event
  let result = d.flush(1150UL)
  assert_true(result is Some(_))
  match result {
    Some(ev) => assert_eq(ev.path, "b.js")
    None => ()
  }
}

///|
test "Debouncer zero delay flushes immediately" {
  let d = Debouncer::new(0)
  let e = wb_event(@chokidar.EventName::Change, "a.js")
  d.feed(e, 1000UL)
  assert_true(d.flush(1000UL) is Some(_))
}

///|
test "Debouncer flush resets state" {
  let d = Debouncer::new(0)
  d.feed(wb_event(@chokidar.EventName::Change, "a.js"), 500UL)
  assert_true(d.flush(500UL) is Some(_))
  assert_true(d.flush(600UL) is None)
}

// ── CallbackQueue unit tests ──────────────────────────────────────────────────

///|
test "CallbackQueue runs callback when idle" {
  let q = CallbackQueue::new(true)
  let calls : Array[String] = []
  q.try_run(
    wb_event(@chokidar.EventName::Change, "a.js"),
    event => calls.push(event.path),
    _ => (),
  )
  assert_eq(calls, ["a.js"])
}

///|
test "CallbackQueue drain replays outer callback on queued event" {
  // When a second event is queued while the first cb runs,
  // the drain fires the OUTER callback (not the inner try_run's cb).
  let q = CallbackQueue::new(true)
  let seen : Array[String] = []
  let e2 = wb_event(@chokidar.EventName::Change, "b.js")
  let did_queue = [false]
  // Outer cb: records path, and while running queues e2 (only once)
  q.try_run(
    wb_event(@chokidar.EventName::Change, "a.js"),
    event => {
      seen.push(event.path)
      if !did_queue[0] {
        did_queue[0] = true
        // This try_run queues e2; the inner cb is ignored by the drain
        q.try_run(e2, event2 => seen.push("ignored:" + event2.path), _ => ())
      }
    },
    _ => (),
  )
  // After outer cb returns: e2 drained with outer cb → sees "b.js"
  assert_eq(seen, ["a.js", "b.js"])
}

///|
test "CallbackQueue drops event when busy queue_mode false" {
  let q = CallbackQueue::new(false)
  let seen : Array[String] = []
  let e2 = wb_event(@chokidar.EventName::Change, "b.js")
  q.try_run(
    wb_event(@chokidar.EventName::Change, "a.js"),
    event => {
      seen.push(event.path)
      q.try_run(e2, event2 => seen.push(event2.path), _ => ())
    },
    _ => (),
  )
  // e2 dropped, only a.js seen
  assert_eq(seen, ["a.js"])
}

///|
test "CallbackQueue forwards callback errors to on_err" {
  let q = CallbackQueue::new(true)
  let errors : Array[String] = []
  q.try_run(
    wb_event(@chokidar.EventName::Change, "a.js"),
    _ => raise GlobError("boom"),
    msg => errors.push(msg),
  )
  assert_false(errors.is_empty())
}

///|
test "CallbackQueue is_running false when idle" {
  let q = CallbackQueue::new(true)
  assert_false(q.is_running())
}

// ── GlobWatcher integration tests ────────────────────────────────────────────

///|
test "GlobWatcher raises GlobError for negation-only pattern" {
  let opts = Options::default()
  try {
    GlobWatcher::new(@chokidar.WatchPaths::One("!**/*.js"), opts) |> ignore
  } catch {
    GlobError(_) => ()
    SetupError(_) => assert_false(true)
  } noraise {
    _ => assert_false(true)
  }
}

///|
test "GlobWatcher ignore_initial suppresses existing files" {
  let root = "./_build/gw-wb-ignore-initial"
  wb_rmtree(root)
  wb_mkdir(root)
  wb_write(root + "/existing.js", "v1")
  let opts = {
    ..Options::default(),
    delay: 0,
    ignore_initial: true,
    cwd: Some(root),
    use_polling: false,
  }
  let w = GlobWatcher::new(@chokidar.WatchPaths::One("**/*.js"), opts)
  let seen : Array[@chokidar.EventName] = []
  w.tick(cb=event => seen.push(event.name))
  w.close()
  wb_rmtree(root)
  assert_true(seen.is_empty())
}

///|
test "GlobWatcher detects initial file with ignore_initial false" {
  let root = "./_build/gw-wb-add"
  wb_rmtree(root)
  wb_mkdir(root)
  wb_write(root + "/new.js", "hello")
  let opts = {
    ..Options::default(),
    delay: 0,
    ignore_initial: false,
    cwd: Some(root),
    use_polling: false,
  }
  let w = GlobWatcher::new(@chokidar.WatchPaths::One("**/*.js"), opts)
  let seen : Array[@chokidar.EventName] = []
  w.tick(cb=event => seen.push(event.name))
  w.close()
  wb_rmtree(root)
  assert_true(seen.contains(@chokidar.EventName::Add))
}

///|
test "GlobWatcher glob filter excludes non-matching files" {
  let root = "./_build/gw-wb-filter"
  wb_rmtree(root)
  wb_mkdir(root)
  wb_write(root + "/notes.txt", "not js")
  let opts = {
    ..Options::default(),
    delay: 0,
    ignore_initial: false,
    cwd: Some(root),
    use_polling: false,
  }
  let w = GlobWatcher::new(@chokidar.WatchPaths::One("**/*.js"), opts)
  let seen : Array[String] = []
  w.tick(cb=event => seen.push(event.path))
  w.close()
  wb_rmtree(root)
  assert_true(seen.is_empty())
}

///|
test "GlobWatcher detects file change via polling" {
  let root = "./_build/gw-wb-change"
  wb_rmtree(root)
  wb_mkdir(root)
  wb_write(root + "/test.js", "v1")
  let opts = {
    ..Options::default(),
    delay: 0,
    ignore_initial: true,
    cwd: Some(root),
    use_polling: true,
  }
  let w = GlobWatcher::new(@chokidar.WatchPaths::One("**/*.js"), opts)
  let seen : Array[String] = []
  // Establish baseline (initial scan)
  w.tick()
  // Modify file; polling rescans on the next tick so no sleep needed
  wb_write(root + "/test.js", "v2")
  w.tick(cb=event => seen.push(event.path))
  w.close()
  wb_rmtree(root)
  assert_false(seen.is_empty())
}

///|
test "GlobWatcher on registers listener for Ready event" {
  let root = "./_build/gw-wb-ready"
  wb_rmtree(root)
  wb_mkdir(root)
  let opts = {
    ..Options::default(),
    delay: 0,
    cwd: Some(root),
    use_polling: false,
  }
  let w = GlobWatcher::new(@chokidar.WatchPaths::One("**/*.js"), opts)
  let ready_fired : Array[Unit] = []
  w.on(@chokidar.EventName::Ready, _ => ready_fired.push(())) |> ignore
  w.tick()
  w.close()
  wb_rmtree(root)
  assert_false(ready_fired.is_empty())
}

///|
test "GlobWatcher close clears listeners" {
  let root = "./_build/gw-wb-close"
  wb_rmtree(root)
  wb_mkdir(root)
  let opts = {
    ..Options::default(),
    delay: 0,
    cwd: Some(root),
    use_polling: false,
  }
  let w = GlobWatcher::new(@chokidar.WatchPaths::One("**/*.js"), opts)
  w.on(@chokidar.EventName::Ready, _ => ()) |> ignore
  assert_false(w.listeners.is_empty())
  w.close()
  assert_true(w.listeners.is_empty())
  wb_rmtree(root)
}

///|
test "GlobWatcher negation glob excludes matched file" {
  let root = "./_build/gw-wb-negation"
  wb_rmtree(root)
  wb_mkdir(root)
  wb_write(root + "/index.js", "public")
  wb_write(root + "/secret.js", "private")
  let opts = {
    ..Options::default(),
    delay: 0,
    ignore_initial: false,
    cwd: Some(root),
    use_polling: false,
  }
  let w = GlobWatcher::new(
    @chokidar.WatchPaths::Many(["**/*.js", "!secret.js"]),
    opts,
  )
  let seen : Array[String] = []
  w.tick(cb=event => seen.push(event.path))
  w.close()
  wb_rmtree(root)
  assert_false(seen.contains("secret.js"))
  assert_true(seen.contains("index.js"))
}

///|
test "GlobWatcher events option filters by event type" {
  let root = "./_build/gw-wb-events-filter"
  wb_rmtree(root)
  wb_mkdir(root)
  let opts = {
    ..Options::default(),
    delay: 0,
    ignore_initial: false,
    events: [@chokidar.EventName::Change],
    cwd: Some(root),
    use_polling: false,
  }
  let w = GlobWatcher::new(@chokidar.WatchPaths::One("**/*.js"), opts)
  let seen : Array[@chokidar.EventName] = []
  wb_write(root + "/new.js", "hello")
  w.tick(cb=event => seen.push(event.name))
  w.close()
  wb_rmtree(root)
  // Add events are filtered out because events=[Change]
  assert_false(seen.contains(@chokidar.EventName::Add))
}

///|
test "GlobWatcher get_watched returns non-empty map" {
  let root = "./_build/gw-wb-watched"
  wb_rmtree(root)
  wb_mkdir(root)
  let opts = {
    ..Options::default(),
    delay: 0,
    cwd: Some(root),
    use_polling: false,
  }
  let w = GlobWatcher::new(@chokidar.WatchPaths::One("**/*.js"), opts)
  let watched = w.get_watched()
  assert_false(watched.is_empty())
  w.close()
  wb_rmtree(root)
}

///|
test "GlobWatcher WatchPaths::Many detects events from all patterns" {
  let root = "./_build/gw-wb-many"
  wb_rmtree(root)
  wb_mkdir(root)
  let opts = {
    ..Options::default(),
    delay:          0,
    ignore_initial: true,
    cwd:            Some(root),
    use_polling:    true,
  }
  let w = GlobWatcher::new(
    @chokidar.WatchPaths::Many(["**/*.js", "**/*.ts"]),
    opts,
  )
  let seen : Array[String] = []
  // Establish baseline with ignore_initial=true
  w.tick()
  // Add .js file; polling detects it on the next tick
  wb_write(root + "/a.js", "js")
  w.tick(cb=event => seen.push(event.path))
  // Add .ts file; detected on the following tick
  wb_write(root + "/b.ts", "ts")
  w.tick(cb=event => seen.push(event.path))
  w.close()
  wb_rmtree(root)
  assert_true(seen.contains("a.js"))
  assert_true(seen.contains("b.ts"))
}

///|
test "GlobWatcher cwd=None watches relative path" {
  let root = "./_build/gw-wb-nocwd"
  wb_rmtree(root)
  wb_mkdir(root)
  wb_write(root + "/hello.js", "hi")
  let opts = {
    ..Options::default(),
    delay:          0,
    ignore_initial: false,
    cwd:            None,
    use_polling:    false,
  }
  let w = GlobWatcher::new(
    @chokidar.WatchPaths::One(root + "/**/*.js"),
    opts,
  )
  let seen : Array[String] = []
  w.tick(cb=event => seen.push(event.path))
  w.close()
  wb_rmtree(root)
  // Event path is relative to process CWD when cwd=None.
  assert_true(seen.any(fn(p) { p.has_suffix("hello.js") }))
}

///|
test "GlobWatcher ignored option excludes via MatchFunction" {
  let root = "./_build/gw-wb-ignored-opt"
  wb_rmtree(root)
  wb_mkdir(root)
  wb_write(root + "/keep.js", "keep")
  wb_write(root + "/skip.js", "skip")
  let opts = {
    ..Options::default(),
    delay:          0,
    ignore_initial: false,
    cwd:            Some(root),
    use_polling:    false,
    ignored: [
      @chokidar.Matcher::MatchFunction(fn(path, _) { path == "skip.js" }),
    ],
  }
  let w = GlobWatcher::new(@chokidar.WatchPaths::One("**/*.js"), opts)
  let seen : Array[String] = []
  w.tick(cb=event => seen.push(event.path))
  w.close()
  wb_rmtree(root)
  assert_true(seen.contains("keep.js"))
  assert_false(seen.contains("skip.js"))
}

///|
test "GlobWatcher depth=0 limits scan to root-level files" {
  let root = "./_build/gw-wb-depth"
  wb_rmtree(root)
  wb_mkdir(root)
  wb_mkdir(root + "/sub")
  wb_write(root + "/root.js", "root")
  wb_write(root + "/sub/deep.js", "deep")
  let opts = {
    ..Options::default(),
    delay:          0,
    ignore_initial: false,
    cwd:            Some(root),
    depth:          0,
    use_polling:    false,
  }
  let w = GlobWatcher::new(@chokidar.WatchPaths::One("**/*.js"), opts)
  let seen : Array[String] = []
  w.tick(cb=event => seen.push(event.path))
  w.close()
  wb_rmtree(root)
  assert_true(seen.contains("root.js"))
  assert_false(seen.contains("sub/deep.js"))
}

///|
test "Debouncer last event in window replaces earlier event" {
  let d = Debouncer::new(100)
  let e1 = wb_event(@chokidar.EventName::Change, "a.js")
  let e2 = wb_event(@chokidar.EventName::Change, "b.js")
  let t0 : UInt64 = 1000UL
  d.feed(e1, t0)
  d.feed(e2, t0)
  // Before delay elapses → nothing
  assert_true(d.flush(t0 + 50UL) is None)
  // After delay → last event wins
  let result = d.flush(t0 + 200UL)
  assert_true(result is Some(_))
  assert_eq(result.unwrap().path, "b.js")
}
