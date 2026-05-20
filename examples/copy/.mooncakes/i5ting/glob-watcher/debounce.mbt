///|
/// Timestamp-based debounce. Integrates with the polling loop.
/// Feed events as they arrive; flush() returns the pending event
/// once `delay` ms have elapsed since the last feed.
struct Debouncer {
  delay            : Int
  mut last_event_at : UInt64?
  mut pending       : @chokidar.Event?
}

///|
fn Debouncer::new(delay : Int) -> Debouncer {
  { delay, last_event_at: None, pending: None }
}

///| Record a file event; resets the debounce timer.
fn Debouncer::feed(
  self  : Debouncer,
  event : @chokidar.Event,
  now   : UInt64,
) -> Unit {
  self.last_event_at = Some(now)
  self.pending = Some(event)
}

///| Return the pending event if `delay` ms have elapsed since the last feed,
/// then reset state. Returns None if still within the debounce window.
fn Debouncer::flush(self : Debouncer, now : UInt64) -> @chokidar.Event? {
  match self.last_event_at {
    None => None
    Some(last) => {
      let delay_u64 = self.delay.reinterpret_as_uint().to_uint64()
      if now >= last + delay_u64 {
        let event = self.pending
        self.last_event_at = None
        self.pending = None
        event
      } else {
        None
      }
    }
  }
}

///| True if a feed has been recorded but not yet flushed.
fn Debouncer::is_pending(self : Debouncer) -> Bool {
  self.pending is Some(_)
}
