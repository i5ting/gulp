///|
struct Debouncer {
  delay_ms : Int64
  mut deadline_ms : Int64?
}

///|
pub fn debouncer(delay_ms : Int64) -> Debouncer {
  { delay_ms, deadline_ms: None }
}

///|
pub fn Debouncer::push(self : Debouncer, now_ms : Int64) -> Unit {
  self.deadline_ms = Some(now_ms + self.delay_ms)
}

///|
pub fn Debouncer::take_ready(self : Debouncer, now_ms : Int64) -> Bool {
  match self.deadline_ms {
    Some(deadline) =>
      if now_ms >= deadline {
        self.deadline_ms = None
        true
      } else {
        false
      }
    None => false
  }
}
