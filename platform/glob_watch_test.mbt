///|
fn render_events(events : Array[GlobWatchEvent]) -> String {
  let rendered : Array[String] = []
  for event in events {
    rendered.push(event.render())
  }
  rendered.join(",")
}

///|
test "glob watch reports added and removed matching paths" {
  let watch = watch_glob(["src/app.mbt", "README.md"], "src/**/*.mbt")
  let events = watch.poll(["src/lib.mbt", "README.md", "test/app.mbt"])
  inspect(render_events(events), content="added:src/lib.mbt,removed:src/app.mbt")
}

///|
test "glob watch normalizes windows separators and case" {
  let watch = watch_glob_on(Windows, ["src\\App.mbt"], "src/**/*.mbt")
  let events = watch.poll(["SRC/app.mbt", "src\\lib.mbt"])
  inspect(render_events(events), content="added:src/lib.mbt")
}

///|
test "glob watch keeps linux case-sensitive event identity" {
  let watch = watch_glob_on(Linux, ["src/App.mbt"], "src/**/*.mbt")
  let events = watch.poll(["src/app.mbt"])
  inspect(render_events(events), content="added:src/app.mbt,removed:src/App.mbt")
}

///|
test "glob watch treats macos case-only changes as same path" {
  let watch = watch_glob_on(MacOS, ["src/App.mbt"], "src/**/*.mbt")
  let events = watch.poll(["src/app.mbt"])
  inspect(render_events(events), content="")
}

///|
test "glob watch events normalize to watch event names" {
  let watch = watch_glob(["src/app.mbt"], "src/**/*.mbt")
  let events = watch.poll(["src/lib.mbt"])
  let normalized = normalize_glob_watch_events(events, ready=true)
  inspect(render_watch_events(normalized), content="add:src/lib.mbt,unlink:src/app.mbt,ready")
}

///|
test "glob watch supports include and exclude rules" {
  let watch = watch_globs(
    ["src/app.mbt", "src/app.test.mbt", "README.md"],
    includes=["src/**/*.mbt", "README.md"],
    excludes=["src/**/*.test.mbt"],
  )
  let events = watch.poll([
    "src/app.mbt",
    "src/app.test.mbt",
    "src/lib.mbt",
    "README.md",
    "dist/out.mbt",
  ])
  inspect(render_events(events), content="added:src/lib.mbt")
}

///|
test "glob watch ignores excluded removals" {
  let watch = watch_globs(
    ["src/app.mbt", "src/app.test.mbt"],
    includes=["src/**/*.mbt"],
    excludes=["src/**/*.test.mbt"],
  )
  let events = watch.poll(["src/app.test.mbt"])
  inspect(render_events(events), content="removed:src/app.mbt")
}
