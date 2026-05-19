///|
struct CancellationToken {
  mut cancelled : Bool
}

///|
pub fn new_cancellation_token() -> CancellationToken {
  { cancelled: false }
}

///|
pub fn CancellationToken::cancel(self : CancellationToken) -> Unit {
  self.cancelled = true
}

///|
pub fn CancellationToken::is_cancelled(self : CancellationToken) -> Bool {
  self.cancelled
}

///|
pub struct Context {
  cwd : String
  now_ms : Int64
  cancellation : CancellationToken
}

///|
pub fn new_context(
  cwd~ : String,
  now_ms~ : Int64,
  cancellation~ : CancellationToken,
) -> Context {
  { cwd, now_ms, cancellation }
}

///|
pub fn Context::is_cancelled(self : Context) -> Bool {
  self.cancellation.is_cancelled()
}
