///|
#borrow(name, watched, path)
extern "C" fn native_backend_enqueue_raw_event(
  name : Bytes,
  watched : Bytes,
  path : Bytes,
) -> Unit = "chokidar_backend_enqueue_raw_event"

///|
fn backend_push_event_for_test(
  name : String,
  watched : String,
  path : String?,
) -> Unit {
  native_backend_enqueue_raw_event(
    backend_string_to_utf8_bytes(name),
    backend_string_to_utf8_bytes(watched),
    backend_string_to_utf8_bytes(path.unwrap_or("")),
  )
}

///|
test "backend_watch with polling returns a valid handle" {
  let options = Options::{ ..Options::default(), use_polling: true }
  let handle = backend_watch("./_build", options)
  assert_true(handle.id >= 0)
  backend_close(handle)
}

///|
test "backend_watch with native returns a valid handle" {
  let options = Options::{ ..Options::default(), use_polling: false }
  let handle = backend_watch("./_build", options)
  assert_true(handle.id >= 0)
  backend_close(handle)
}

///|
test "backend drains raw events from native queue" {
  backend_drain_events() |> ignore
  backend_push_event_for_test("change", "./_build", Some("a.txt"))
  let events = backend_drain_events()
  inspect(events.length(), content="1")
  inspect(events[0].backend_event_name, content="change")
  inspect(events[0].watched_path, content="./_build")
  assert_true(events[0].event_path == Some("a.txt"))
  inspect(backend_drain_events().length(), content="0")
}

///|
test "backend decodes utf8 event paths" {
  backend_drain_events() |> ignore
  backend_push_event_for_test("change", "./_build", Some("你好.txt"))
  let events = backend_drain_events()
  inspect(events.length(), content="1")
  assert_true(events[0].event_path == Some("你好.txt"))
}

///|
test "backend reports event queue overflow" {
  backend_drain_events() |> ignore
  for index in 0..<1025 {
    backend_push_event_for_test(
      "change",
      "./_build",
      Some("event-\{index}.txt"),
    )
  }
  let events = backend_drain_events()
  let mut saw_overflow = false
  for event in events {
    if event.backend_event_name == "overflow" {
      saw_overflow = true
      assert_true(event.error_message == Some("backend event queue overflow"))
    }
  }
  assert_true(saw_overflow)
  inspect(backend_drain_events().length(), content="0")
}
