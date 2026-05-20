///|
#borrow(path)
extern "C" fn native_backend_watch(
  path : Bytes,
  use_polling : Int,
  interval : Int,
) -> Int = "chokidar_backend_watch"

///|
extern "C" fn native_backend_close(id : Int) -> Unit = "chokidar_backend_close"

///|
extern "C" fn native_backend_event_count() -> Int = "chokidar_backend_event_count"

///|
extern "C" fn native_backend_event_name(index : Int) -> Bytes = "chokidar_backend_event_name"

///|
extern "C" fn native_backend_watched_path(index : Int) -> Bytes = "chokidar_backend_watched_path"

///|
extern "C" fn native_backend_event_path(index : Int) -> Bytes = "chokidar_backend_event_path"

///|
extern "C" fn native_backend_platform() -> Bytes = "chokidar_backend_platform"

///|
extern "C" fn native_backend_clear_events() -> Unit = "chokidar_backend_clear_events"

///|
#borrow(name)
extern "C" fn native_getenv_valid(name : Bytes) -> Int = "chokidar_getenv_valid"

///|
#borrow(name)
extern "C" fn native_getenv_value(name : Bytes) -> Bytes = "chokidar_getenv_value"

///|
fn chokidar_getenv(name : String) -> String? {
  let name_bytes = backend_string_to_utf8_bytes(name)
  if native_getenv_valid(name_bytes) == 0 {
    None
  } else {
    Some(backend_bytes_to_string(native_getenv_value(name_bytes)))
  }
}

///|
fn backend_string_to_utf8_bytes(value : String) -> Bytes {
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
  backend_fixed_byte_array_to_bytes(bytes)
}

///|
fn backend_fixed_byte_array_to_bytes(bytes : FixedArray[Byte]) -> Bytes = "%identity"

///|
fn backend_bytes_to_string(value : Bytes) -> String {
  // Native path bytes become public MoonBit path strings; invalid UTF-8 is replaced.
  @utf8.decode_lossy(value)
}

///|
fn backend_watch(
  path : String,
  options : Options,
) -> BackendHandle raise ChokidarError {
  let id = native_backend_watch(
    backend_string_to_utf8_bytes(path),
    if options.use_polling {
      1
    } else {
      0
    },
    options.interval,
  )
  guard id >= 0 else { raise ChokidarError("failed to start backend watcher") }
  { id, }
}

///|
fn backend_close(handle : BackendHandle) -> Unit {
  native_backend_close(handle.id)
}

///|
fn backend_drain_events() -> Array[RawEvent] {
  let count = native_backend_event_count()
  let events : Array[RawEvent] = []
  for index in 0..<count {
    let event_name = backend_bytes_to_string(native_backend_event_name(index))
    let raw_path = backend_bytes_to_string(native_backend_event_path(index))
    events.push({
      backend_event_name: event_name,
      watched_path: backend_bytes_to_string(native_backend_watched_path(index)),
      event_path: if raw_path == "" {
        None
      } else {
        Some(raw_path)
      },
      platform: backend_bytes_to_string(native_backend_platform()),
      error_message: if event_name == "overflow" {
        Some("backend event queue overflow")
      } else {
        None
      },
    })
  }
  native_backend_clear_events()
  events
}

