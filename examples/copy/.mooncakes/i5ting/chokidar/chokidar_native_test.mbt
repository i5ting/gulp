///|
#borrow(path)
extern "C" fn raw_native_test_create_dir(path : Bytes) -> Int = "readdirp_test_create_dir"

///|
#borrow(path, contents)
extern "C" fn raw_native_test_write_file(path : Bytes, contents : Bytes) -> Int = "readdirp_test_write_file"

///|
#borrow(path)
extern "C" fn raw_native_test_remove_tree(path : Bytes) -> Int = "readdirp_test_remove_tree"

///|
#borrow(name, watched, path)
extern "C" fn native_backend_enqueue_raw_event(
  name : Bytes,
  watched : Bytes,
  path : Bytes,
) -> Unit = "chokidar_backend_enqueue_raw_event"

///|
fn native_test_string_to_utf8_bytes(value : String) -> Bytes {
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
  native_test_fixed_byte_array_to_bytes(bytes)
}

///|
fn native_test_fixed_byte_array_to_bytes(bytes : FixedArray[Byte]) -> Bytes = "%identity"

///|
fn native_test_create_dir(path : String) -> Unit raise @chokidar.ChokidarError {
  guard raw_native_test_create_dir(native_test_string_to_utf8_bytes(path)) == 0 else {
    raise @chokidar.ChokidarError("failed to create test directory: " + path)
  }
}

///|
fn native_test_write_file(
  path : String,
  contents : String,
) -> Unit raise @chokidar.ChokidarError {
  guard raw_native_test_write_file(
      native_test_string_to_utf8_bytes(path),
      native_test_string_to_utf8_bytes(contents),
    ) ==
    0 else {
    raise @chokidar.ChokidarError("failed to write test file: " + path)
  }
}

///|
fn native_test_remove_tree(path : String) -> Unit raise @chokidar.ChokidarError {
  guard raw_native_test_remove_tree(native_test_string_to_utf8_bytes(path)) == 0 else {
    raise @chokidar.ChokidarError("failed to remove test tree: " + path)
  }
}

///|
fn native_test_push_raw_event(
  name : String,
  watched : String,
  path : String?,
) -> Unit {
  native_backend_enqueue_raw_event(
    native_test_string_to_utf8_bytes(name),
    native_test_string_to_utf8_bytes(watched),
    native_test_string_to_utf8_bytes(path.unwrap_or("")),
  )
}

///|
fn native_test_sleep_millis(millis : Int) -> Unit {
  let deadline = @env.now() + millis.reinterpret_as_uint().to_uint64()
  while @env.now() < deadline {
    ()
  }
}

///|
test "watch emits initial add events and ready" {
  let root = "./_build/readdirp-chokidar-watch-initial-api"
  native_test_remove_tree(root)
  native_test_create_dir(root)
  native_test_write_file(root + "/a.txt", "a")
  let seen : Array[@chokidar.EventName] = []
  let watcher = @chokidar.watch(@chokidar.One(root), options={
    ..@chokidar.Options::default(),
    cwd: Some(root),
    use_polling: true,
  })
  watcher
  .on(@chokidar.All, event => seen.push(event.name))
  .on(@chokidar.Ready, event => seen.push(event.name))
  |> ignore
  watcher.process()
  assert_true(seen.contains(@chokidar.Add))
  assert_true(seen.contains(@chokidar.Ready))
  watcher.close()
  native_test_remove_tree(root)
}

///|
test "watch detects add change and unlink after raw pulse" {
  let root = "./_build/readdirp-chokidar-watch-diff-api"
  native_test_remove_tree(root)
  native_test_create_dir(root)
  let watcher = @chokidar.watch(@chokidar.One(root), options={
    ..@chokidar.Options::default(),
    cwd: Some(root),
    ignore_initial: true,
    use_polling: true,
    atomic_option: @chokidar.AtomicOff,
  })
  let seen : Array[String] = []
  watcher.on(@chokidar.All, event => {
    seen.push(event.name.to_string() + ":" + event.path)
  })
  |> ignore
  watcher.process()
  native_test_write_file(root + "/a.txt", "a")
  native_test_push_raw_event("change", root, None)
  watcher.process()
  native_test_write_file(root + "/a.txt", "aa")
  native_test_push_raw_event("change", root, Some("a.txt"))
  watcher.process()
  native_test_remove_tree(root + "/a.txt")
  native_test_push_raw_event("change", root, Some("a.txt"))
  watcher.process()
  assert_true(seen.contains("Add:a.txt"))
  assert_true(seen.contains("Change:a.txt"))
  assert_true(seen.contains("Unlink:a.txt"))
  watcher.close()
  native_test_remove_tree(root)
}

///|
test "watcher add propagates scanner errors" {
  let root = "./_build/readdirp-chokidar-watch-scan-error-api"
  native_test_remove_tree(root)
  native_test_create_dir(root)
  try @chokidar.watch(@chokidar.One(root), options={
    ..@chokidar.Options::default(),
    depth: -1,
    use_polling: true,
  }) |> ignore catch {
    @chokidar.ChokidarError(message) =>
      inspect(message, content="depth must be non-negative")
  } noraise {
    _ => assert_false(true)
  }
  native_test_remove_tree(root)
}

///|
test "native backend detects file creation without polling" {
  let root = "./_build/readdirp-chokidar-native-backend"
  native_test_remove_tree(root)
  native_test_create_dir(root)
  let watcher = @chokidar.watch(@chokidar.One(root), options={
    ..@chokidar.Options::default(),
    ignore_initial: true,
    use_polling: false,
  })
  let seen : Array[@chokidar.EventName] = []
  watcher.on(@chokidar.All, event => seen.push(event.name)) |> ignore
  watcher.process()
  native_test_write_file(root + "/a.txt", "hello")
  native_test_sleep_millis(50)
  watcher.process()
  assert_true(seen.contains(@chokidar.Add))
  watcher.close()
  native_test_remove_tree(root)
}

///|
test "await write finish allows add once stats are available" {
  let root = "./_build/readdirp-chokidar-await-write-api"
  native_test_remove_tree(root)
  native_test_create_dir(root)
  let watcher = @chokidar.watch(@chokidar.One(root), options={
    ..@chokidar.Options::default(),
    ignore_initial: true,
    use_polling: true,
    await_write_finish: @chokidar.AwaitWriteFinishOn({
      stability_threshold: 1,
      poll_interval: 1,
    }),
  })
  let seen : Array[@chokidar.EventName] = []
  watcher.on(@chokidar.All, event => seen.push(event.name)) |> ignore
  watcher.process()
  native_test_write_file(root + "/a.txt", "a")
  native_test_push_raw_event("change", root, Some("a.txt"))
  watcher.process()
  assert_false(seen.contains(@chokidar.Add))
  native_test_sleep_millis(5)
  watcher.process()
  assert_true(seen.contains(@chokidar.Add))
  watcher.close()
  native_test_remove_tree(root)
}
