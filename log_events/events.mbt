///|
pub(all) enum LogEvent {
  TaskStart(String)
  TaskStop(String, Int64)
  TaskError(String, String)
}

///|
pub struct LogEventRecorder {
  events : Array[LogEvent]
}

///|
pub fn task_start(name : String) -> LogEvent {
  TaskStart(name)
}

///|
pub fn task_stop(name : String, duration_ms~ : Int64) -> LogEvent {
  TaskStop(name, duration_ms)
}

///|
pub fn task_error(name : String, detail : String) -> LogEvent {
  TaskError(name, detail)
}

///|
pub fn LogEvent::render(self : LogEvent) -> String {
  match self {
    TaskStart(name) => "[mulp] Starting '" + name + "'..."
    TaskStop(name, duration_ms) =>
      "[mulp] Finished '" + name + "' after " + duration_ms.to_string() + " ms"
    TaskError(name, detail) => "[mulp] '" + name + "' errored: " + detail
  }
}

///|
pub fn LogEvent::render_with_time(self : LogEvent, time : String) -> String {
  let prefix = "[" + time + "] "
  match self {
    TaskStart(name) => prefix + "Starting '" + name + "'..."
    TaskStop(name, duration_ms) =>
      prefix + "Finished '" + name + "' after " + duration_ms.to_string() + " ms"
    TaskError(name, detail) => prefix + "'" + name + "' errored: " + detail
  }
}

///|
pub fn render_log_events(events : Array[LogEvent]) -> String {
  let rendered : Array[String] = []
  for event in events {
    rendered.push(event.render())
  }
  rendered.join("\n")
}

///|
pub fn new_log_event_recorder() -> LogEventRecorder {
  { events: [] }
}

///|
pub fn LogEventRecorder::emit(self : LogEventRecorder, event : LogEvent) -> Unit {
  self.events.push(event)
}

///|
pub fn LogEventRecorder::events(self : LogEventRecorder) -> Array[LogEvent] {
  let copied : Array[LogEvent] = []
  for event in self.events {
    copied.push(event)
  }
  copied
}

///|
pub fn LogEventRecorder::render(self : LogEventRecorder) -> String {
  render_log_events(self.events)
}
