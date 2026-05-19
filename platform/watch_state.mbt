///|
struct WatchRunState {
  mut active : Bool
  mut running : Bool
  mut pending_rebuild : Bool
  mut last_failure : String?
}

///|
pub fn watch_run_state() -> WatchRunState {
  { active: true, running: false, pending_rebuild: false, last_failure: None }
}

///|
pub fn WatchRunState::start_run(self : WatchRunState) -> Unit {
  self.running = true
  self.last_failure = None
}

///|
pub fn WatchRunState::mark_change(self : WatchRunState) -> Unit {
  if self.running {
    self.pending_rebuild = true
  }
}

///|
pub fn WatchRunState::has_pending_rebuild(self : WatchRunState) -> Bool {
  self.pending_rebuild
}

///|
pub fn WatchRunState::is_active(self : WatchRunState) -> Bool {
  self.active
}

///|
pub fn WatchRunState::is_running(self : WatchRunState) -> Bool {
  self.running
}

///|
pub fn WatchRunState::last_failure(self : WatchRunState) -> String? {
  self.last_failure
}

///|
pub fn WatchRunState::finish_run(self : WatchRunState) -> Bool {
  self.running = false
  let pending = self.pending_rebuild
  self.pending_rebuild = false
  pending
}

///|
pub fn WatchRunState::finish_failed_run(
  self : WatchRunState,
  message : String,
) -> Bool {
  self.last_failure = Some(message)
  self.finish_run()
}

///|
pub fn WatchRunState::stop(
  self : WatchRunState,
  cancellation : @core.CancellationToken,
) -> Unit {
  cancellation.cancel()
  self.active = false
  self.running = false
  self.pending_rebuild = false
}
