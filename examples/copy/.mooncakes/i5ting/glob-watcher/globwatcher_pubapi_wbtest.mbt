// ── Public API smoke tests (no filesystem) ───────────────────────────────────

///|
test "Options::default has expected values" {
  let opts = Options::default()
  inspect(opts.delay, content="200")
  inspect(opts.queue, content="true")
  inspect(opts.ignore_initial, content="true")
  inspect(opts.depth, content="2147483647")
  inspect(opts.loop_interval, content="10")
  inspect(opts.use_polling, content="true")
  assert_true(opts.cwd is None)
  assert_true(opts.ignored.is_empty())
  inspect(opts.events.length(), content="3")
  assert_true(opts.events.contains(@chokidar.EventName::Add))
  assert_true(opts.events.contains(@chokidar.EventName::Change))
  assert_true(opts.events.contains(@chokidar.EventName::Unlink))
}

///|
test "GlobWatcher::new raises GlobError for negation-only pattern" {
  let opts = Options::default()
  try {
    GlobWatcher::new(
      @chokidar.WatchPaths::One("!**/*.js"),
      opts,
    )
    |> ignore
  } catch {
    GlobError(_) => ()
    SetupError(_) => assert_false(true)
  } noraise {
    _ => assert_false(true)
  }
}

///|
test "GlobWatcher::new raises GlobError for multiple negation-only patterns" {
  let opts = Options::default()
  try {
    GlobWatcher::new(
      @chokidar.WatchPaths::Many(["!vendor/**", "!node_modules/**"]),
      opts,
    )
    |> ignore
  } catch {
    GlobError(_) => ()
    SetupError(_) => assert_false(true)
  } noraise {
    _ => assert_false(true)
  }
}

///|
test "glob_match star within single segment" {
  assert_true(glob_match("*.js", "index.js"))
  assert_false(glob_match("*.js", "lib/index.js"))
}

///|
test "glob_match double star across segments" {
  assert_true(glob_match("**/*.js", "lib/util/index.js"))
  assert_true(glob_match("**/*.js", "index.js"))
}

///|
test "glob_match brace expansion" {
  assert_true(glob_match("*.{js,ts}", "index.js"))
  assert_true(glob_match("*.{js,ts}", "index.ts"))
  assert_false(glob_match("*.{js,ts}", "index.py"))
}

///|
test "parse_globs positives and negations separate" {
  let (pos, neg) = parse_globs(["**/*.js", "!vendor/**"])
  assert_eq(pos.length(), 1)
  assert_eq(neg.length(), 1)
}

///|
test "parse_globs re-add removes matching negation" {
  let (_, neg) = parse_globs([
    "**/*.js", "!src/index.js", "src/index.js",
  ])
  assert_true(neg.is_empty())
}

///|
test "GlobPattern parse exposes raw base_dir is_negation" {
  let pats = GlobPattern::parse("src/**/*.js")
  assert_eq(pats.length(), 1)
  assert_eq(pats[0].raw, "src/**/*.js")
  assert_eq(pats[0].base_dir, "src")
  assert_false(pats[0].is_negation)
}

///|
test "GlobPattern parse negation" {
  let pats = GlobPattern::parse("!vendor/**")
  assert_true(pats[0].is_negation)
  assert_eq(pats[0].raw, "vendor/**")
}
