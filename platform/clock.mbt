///|
pub struct ManualClock {
  mut now : Int64
}

///|
pub struct SystemClock {
  marker : Unit
}

///|
pub fn manual_clock(now_ms : Int64) -> ManualClock {
  { now: now_ms }
}

///|
pub fn ManualClock::now_ms(self : ManualClock) -> Int64 {
  self.now
}

///|
pub fn ManualClock::advance_ms(self : ManualClock, delta : Int64) -> Unit {
  self.now = self.now + delta
}

///|
pub fn system_clock() -> SystemClock {
  { marker: () }
}

///|
pub fn SystemClock::now_ms(_self : SystemClock) -> Int64 {
  @env.now().reinterpret_as_int64()
}
