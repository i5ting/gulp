///|
test "watch_options_default has expected values" {
  let opts = default_watch_options()
  inspect(opts.delay, content="200")
  inspect(opts.queue, content="true")
  inspect(opts.ignore_initial, content="true")
}

///|
test "watch_new returns a WatchHandle without blocking" {
  let registry = new_async_registry()
  let ran : Array[String] = []
  let task = async_task(registry, "test", fn(_ctx) {
    ran.push("ran")
    Ok(())
  })
  // Creating a WatchHandle over an existing empty directory with no matching
  // files should not block and should not error.
  let handle = try {
    watch_new(["/tmp/mulp-watch-new-test/**/*.sentinel"], task)
  } catch {
    @glob_watcher.GlobError(msg) => {
      inspect("unexpected GlobError: " + msg, content="no error expected")
      return
    }
    @glob_watcher.SetupError(msg) => {
      inspect("unexpected SetupError: " + msg, content="no error expected")
      return
    }
  }
  handle.close()
  inspect(ran.length(), content="0")
}

///|
test "default_watch_options has expected defaults" {
  let opts = default_watch_options()
  inspect(opts.delay,          content="200")
  inspect(opts.queue,          content="true")
  inspect(opts.ignore_initial, content="true")
  inspect(opts.depth,          content="2147483647")
  inspect(opts.loop_interval,  content="10")
  assert_true(opts.cwd is None)
  inspect(opts.events.length(), content="0")
  inspect(opts.use_polling,    content="true")
}
