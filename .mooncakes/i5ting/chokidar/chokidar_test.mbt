///|
test "api surface matches chokidar-main public shape" {
  let options = @chokidar.Options::default()
  inspect(options.persistent, content="true")
  inspect(options.ignore_initial, content="false")
  inspect(options.follow_symlinks, content="true")
  inspect(options.use_polling, content="false")
  inspect(options.interval, content="100")
  inspect(options.binary_interval, content="300")
  inspect(options.always_stat, content="false")
  inspect(options.depth, content="2147483647")
  inspect(options.ignore_permission_errors, content="false")
  inspect(options.ignored.length(), content="0")
  assert_true(options.cwd is None)
  assert_true(options.atomic_option is @chokidar.AtomicDefault)
  assert_true(@chokidar.AtomicOff is @chokidar.AtomicOff)
  let atomic_window = @chokidar.AtomicWindow(100)
  match atomic_window {
    @chokidar.AtomicWindow(window) => inspect(window, content="100")
    _ => assert_false(true)
  }
  assert_true(options.await_write_finish is @chokidar.AwaitWriteFinishOff)
  match @chokidar.AwaitWriteFinish::default_on() {
    @chokidar.AwaitWriteFinishOn(config) => {
      inspect(config.stability_threshold, content="2000")
      inspect(config.poll_interval, content="100")
    }
    _ => assert_false(true)
  }
  let await_write_finish = @chokidar.AwaitWriteFinishOn({
    stability_threshold: 500,
    poll_interval: 25,
  })
  match await_write_finish {
    @chokidar.AwaitWriteFinishOn(config) => {
      inspect(config.stability_threshold, content="500")
      inspect(config.poll_interval, content="25")
    }
    _ => assert_false(true)
  }
  let events = @chokidar.all_event_names()
  inspect(events.length(), content="9")
  assert_true(events.contains(@chokidar.Add))
  assert_true(events.contains(@chokidar.Change))
  assert_true(events.contains(@chokidar.AddDir))
  assert_true(events.contains(@chokidar.Unlink))
  assert_true(events.contains(@chokidar.UnlinkDir))
  assert_true(events.contains(@chokidar.Ready))
  assert_true(events.contains(@chokidar.Raw))
  assert_true(events.contains(@chokidar.Error))
  assert_true(events.contains(@chokidar.All))
}

///|
test "add validates many paths before mutating watched state" {
  let watcher = @chokidar.watch(@chokidar.One("_build"), options={
    ..@chokidar.Options::default(),
    ignore_initial: true,
  })
  let before = watcher.get_watched().length()
  try watcher.add(@chokidar.Many(["ok.txt", ""])) |> ignore catch {
    @chokidar.ChokidarError(message) =>
      inspect(message, content="watch path must not be empty")
  } noraise {
    _ => assert_false(true)
  }
  assert_true(watcher.get_watched().length() == before)
  watcher.close()
}

///|
test "unwatch removes empty watched directories" {
  let watcher = @chokidar.watch(@chokidar.One("_build"), options={
    ..@chokidar.Options::default(),
    ignore_initial: true,
  })
  inspect(watcher.get_watched(), content="{\".\": [\"_build\"]}")
  watcher.unwatch(@chokidar.One("_build")) |> ignore
  inspect(watcher.get_watched(), content="{}")
  watcher.close()
}

///|
test "close clears watched directories" {
  let watcher = @chokidar.watch(@chokidar.One("_build"), options={
    ..@chokidar.Options::default(),
    ignore_initial: true,
  })
  inspect(watcher.get_watched(), content="{\".\": [\"_build\"]}")
  watcher.close()
  inspect(watcher.get_watched(), content="{}")
}
