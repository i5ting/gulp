///|
fn chokidar_getenv(_name : String) -> String? {
  None
}

///|
fn backend_watch(
  _path : String,
  _options : Options,
) -> BackendHandle raise ChokidarError {
  { id: 0 }
}

///|
fn backend_close(_handle : BackendHandle) -> Unit {
  ()
}

///|
fn backend_drain_events() -> Array[RawEvent] {
  []
}
