///|
/// Mirrors the JS glob-watcher running/queued state machine.
/// Ensures the callback is not invoked concurrently and optionally
/// queues the next run while one is in progress.
struct CallbackQueue {
  mut running      : Bool
  mut queued_event : @chokidar.Event?
  queue_mode       : Bool
}

///|
fn CallbackQueue::new(queue_mode : Bool) -> CallbackQueue {
  { running: false, queued_event: None, queue_mode }
}

///| Attempt to run `cb` with `event`.
/// - If idle: run immediately. After completion, run queued event if any.
/// - If busy and queue_mode=true: store event for next run.
/// - If busy and queue_mode=false: drop silently.
/// Errors from `cb` are forwarded to `on_err`; the queue continues running.
fn CallbackQueue::try_run(
  self   : CallbackQueue,
  event  : @chokidar.Event,
  cb     : (@chokidar.Event) -> Unit raise Error,
  on_err : (String) -> Unit,
) -> Unit {
  if self.running {
    if self.queue_mode {
      self.queued_event = Some(event)
    }
    return
  }
  self.running = true
  let had_error : Array[Bool] = [false]
  let error_msg : Array[String] = [""]
  try {
    cb(event)
  } catch {
    e => {
      had_error[0] = true
      error_msg[0] = e.to_string()
    }
  }
  self.running = false
  if had_error[0] {
    on_err(error_msg[0])
  }
  // Drain one queued event if present.
  match self.queued_event {
    Some(queued) => {
      self.queued_event = None
      self.try_run(queued, cb, on_err)
    }
    None => ()
  }
}

///| True if the callback is currently executing.
fn CallbackQueue::is_running(self : CallbackQueue) -> Bool {
  self.running
}
