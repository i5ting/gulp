///|
pub type TaskRun = (Context) -> Result[Unit, MulpError]

///|
priv struct TaskState {
  mut last_run_ms : Int64?
}

///|
struct Task {
  name : String
  display_name : String?
  description : String?
  exported : Bool
  children : Array[Task]
  run : TaskRun
  state : TaskState
}

///|
pub fn new_task(name : String, run : TaskRun) -> Task {
  {
    name,
    display_name: None,
    description: None,
    exported: true,
    children: [],
    run,
    state: { last_run_ms: None },
  }
}

///|
pub fn new_task_with_metadata(
  name : String,
  run : TaskRun,
  display_name~ : String?,
  description~ : String?,
) -> Task {
  {
    name,
    display_name,
    description,
    exported: true,
    children: [],
    run,
    state: { last_run_ms: None },
  }
}

///|
pub fn new_private_task(name : String, run : TaskRun) -> Task {
  {
    name,
    display_name: None,
    description: None,
    exported: false,
    children: [],
    run,
    state: { last_run_ms: None },
  }
}

///|
fn new_composed_task(name : String, children : Array[Task], run : TaskRun) -> Task {
  {
    name,
    display_name: None,
    description: None,
    exported: true,
    children,
    run,
    state: { last_run_ms: None },
  }
}

///|
pub fn Task::name(self : Task) -> String {
  self.name
}

///|
pub fn Task::display_name(self : Task) -> String? {
  self.display_name
}

///|
pub fn Task::description(self : Task) -> String? {
  self.description
}

///|
pub fn Task::is_private(self : Task) -> Bool {
  !self.exported
}

///|
pub fn Task::children(self : Task) -> Array[Task] {
  self.children
}

///|
pub fn Task::execute(self : Task, ctx : Context) -> Result[Unit, MulpError] {
  if ctx.is_cancelled() {
    Err(task_failed(self.name, "cancelled"))
  } else {
    match (self.run)(ctx) {
      Ok(_) => {
        self.state.last_run_ms = Some(ctx.now_ms)
        Ok(())
      }
      Err(err) => Err(err)
    }
  }
}

///|
pub fn last_run(task : Task) -> Int64? {
  task.state.last_run_ms
}
