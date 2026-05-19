///|
priv struct ListenerEntry {
  event : EventName
  callback : (Event) -> Unit
}

///|
priv struct Emitter {
  listeners : Array[ListenerEntry]
}

///|
fn Emitter::new() -> Emitter {
  { listeners: [] }
}

///|
fn Emitter::on(
  self : Emitter,
  event : EventName,
  callback : (Event) -> Unit,
) -> Unit {
  self.listeners.push({ event, callback })
}

///|
fn Emitter::emit(self : Emitter, event : Event) -> Unit {
  let listeners = self.listeners.copy()
  for entry in listeners {
    if entry.event == event.name {
      (entry.callback)(event)
    }
  }
  if event.name != Ready &&
    event.name != Raw &&
    event.name != Error &&
    event.name != All {
    for entry in listeners {
      if entry.event == All {
        (entry.callback)(event)
      }
    }
  }
}

///|
fn Emitter::off(
  self : Emitter,
  event : EventName,
  callback : (Event) -> Unit,
) -> Unit {
  self.listeners.retain(
    entry =>
      !(entry.event == event &&
        physical_equal(entry.callback, callback)),
  )
}

///|
fn Emitter::remove_all_listeners(self : Emitter) -> Unit {
  self.listeners.clear()
}
