///|
pub type AsyncTaskRun = async (Context) -> Result[Unit, MulpError]

///|
pub type TaskCallback = (Result[Unit, MulpError]) -> Unit

///|
pub type CallbackTaskRegister = async (TaskCallback) -> Unit

///|
pub(all) enum AsyncCompletionSignal {
  SyncCompletion
  StreamCompletion
  CallbackCompletion
  ProcessCompletion
}

///|
struct AsyncTask {
  name : String
  children : Array[AsyncTask]
  run : AsyncTaskRun
  state : TaskState
}

///|
struct AsyncRegistry {
  name : String
  tasks : Array[AsyncTask]
  init : (AsyncRegistry) -> Unit
}

///|
pub fn new_async_task(name : String, run : AsyncTaskRun) -> AsyncTask {
  { name, children: [], run, state: { last_run_ms: None } }
}

///|
fn new_composed_async_task(
  name : String,
  children : Array[AsyncTask],
  run : AsyncTaskRun,
) -> AsyncTask {
  { name, children, run, state: { last_run_ms: None } }
}

///|
pub fn async_task_from_sync(task : Task) -> AsyncTask {
  new_async_task(task.name(), fn(ctx : Context) -> Result[Unit, MulpError] {
    task.execute(ctx)
  })
}

///|
pub fn new_async_registry() -> AsyncRegistry {
  new_async_registry_with_name("default")
}

///|
pub fn new_async_registry_with_name(name : String) -> AsyncRegistry {
  { name, tasks: [], init: fn(_) {} }
}

///|
pub fn new_async_registry_with_init(
  init : (AsyncRegistry) -> Unit,
) -> AsyncRegistry {
  { name: "default", tasks: [], init }
}

///|
pub fn AsyncRegistry::call_init(self : AsyncRegistry) -> Unit {
  (self.init)(self)
}

///|
pub fn AsyncRegistry::all_tasks(self : AsyncRegistry) -> Array[AsyncTask] {
  self.tasks.copy()
}

///|
pub fn AsyncRegistry::set_task(
  self : AsyncRegistry,
  task : AsyncTask,
) -> Unit {
  for i in 0..<self.tasks.length() {
    if self.tasks[i].name() == task.name() {
      self.tasks[i] = task
      return
    }
  }
  self.tasks.push(task)
}

///|
pub fn AsyncRegistry::name(self : AsyncRegistry) -> String {
  self.name
}

///|
pub fn async_task(
  registry : AsyncRegistry,
  name : String,
  run : AsyncTaskRun,
) -> AsyncTask {
  let next = new_async_task(name, run)
  registry.tasks.push(next)
  next
}

///|
pub fn async_task_from(
  registry : AsyncRegistry,
  name : String,
  source : AsyncTask,
) -> AsyncTask {
  let next = new_composed_async_task(
    name,
    [source],
    async fn(ctx : Context) -> Result[Unit, MulpError] {
      source.execute(ctx)
    },
  )
  registry.tasks.push(next)
  next
}

///|
pub fn AsyncRegistry::find(self : AsyncRegistry, name : String) -> AsyncTask? {
  for item in self.tasks {
    if item.name == name {
      return Some(item)
    }
  }
  None
}

///|
pub fn AsyncRegistry::require(
  self : AsyncRegistry,
  name : String,
) -> Result[AsyncTask, MulpError] {
  match self.find(name) {
    Some(found) => Ok(found)
    None => Err(task_not_found(name))
  }
}

///|
pub fn AsyncRegistry::names(self : AsyncRegistry) -> Array[String] {
  let names : Array[String] = []
  for item in self.tasks {
    names.push(item.name)
  }
  names
}

///|
fn async_task_tree(task : AsyncTask) -> TreeNode {
  let children : Array[TreeNode] = []
  for child in task.children() {
    children.push(async_task_tree(child))
  }
  { name: task.name(), children }
}

///|
pub fn AsyncRegistry::tree(self : AsyncRegistry) -> TreeNode {
  let children : Array[TreeNode] = []
  for task in self.tasks {
    children.push(async_task_tree(task))
  }
  { name: "root", children }
}

///|
pub fn async_task_from_callback(
  name : String,
  register : CallbackTaskRegister,
) -> AsyncTask {
  new_async_task(name, async fn(_ctx : Context) -> Result[Unit, MulpError] {
    let completions : Array[Result[Unit, MulpError]] = []
    let ready = @async.CondVar::Cond()
    register(fn(result : Result[Unit, MulpError]) -> Unit {
      completions.push(result)
      ready.signal()
    })
    while completions.length() == 0 {
      ready.wait()
    }
    if completions.length() > 1 {
      Err(task_failed(name, "callback called more than once"))
    } else {
      completions[0]
    }
  })
}

///|
pub fn async_task_with_timeout(
  task : AsyncTask,
  timeout_ms~ : Int,
) -> AsyncTask {
  new_composed_async_task(
    task.name(),
    [task],
    async fn(ctx : Context) -> Result[Unit, MulpError] {
      match @async.with_timeout_opt(timeout_ms, async fn() -> Result[Unit, MulpError] {
        task.execute(ctx)
      }) {
        Some(result) => result
        None => {
          ctx.cancellation.cancel()
          Err(task_failed(task.name(), "timed out after \{timeout_ms}ms"))
        }
      }
    },
  )
}

///|
pub fn validate_async_completion_signals(
  name : String,
  signals : Array[AsyncCompletionSignal],
) -> Result[Unit, MulpError] {
  if signals.length() <= 1 {
    Ok(())
  } else {
    let labels : Array[String] = []
    for signal in signals {
      labels.push(signal.label())
    }
    Err(task_failed(
      name,
      "multiple async completion signals: " + labels.join(", "),
    ))
  }
}

///|
fn AsyncCompletionSignal::label(self : AsyncCompletionSignal) -> String {
  match self {
    SyncCompletion => "sync"
    StreamCompletion => "stream"
    CallbackCompletion => "callback"
    ProcessCompletion => "process"
  }
}

///|
pub fn async_series(tasks : Array[AsyncTask]) -> AsyncTask {
  new_composed_async_task(
    "series",
    tasks,
    async fn(ctx : Context) -> Result[Unit, MulpError] {
      for task in tasks {
        if ctx.is_cancelled() {
          return Err(task_failed(task.name(), "cancelled"))
        }
        match task.execute(ctx) {
          Ok(_) => ()
          Err(err) => return Err(err)
        }
      }
      Ok(())
    },
  )
}

///|
pub fn async_parallel(tasks : Array[AsyncTask]) -> AsyncTask {
  new_composed_async_task(
    "parallel",
    tasks,
    async fn(ctx : Context) -> Result[Unit, MulpError] {
      let errors : Array[(String, String)] = []
      @async.with_task_group(group => {
        for task in tasks {
          group.spawn_bg(allow_failure=true, async fn() -> Unit {
            match task.execute(ctx) {
              Ok(_) => ()
              Err(err) => {
                errors.push((task.name(), err.message()))
                ctx.cancellation.cancel()
              }
            }
          })
        }
      })
      if errors.length() == 0 {
        Ok(())
      } else {
        Err(parallel_failed(errors))
      }
    },
  )
}

///| Like async_parallel but does not short-circuit on error.
///| All tasks run to completion; errors are aggregated.
pub fn async_parallel_settle(tasks : Array[AsyncTask]) -> AsyncTask {
  let name = "parallel_settle(" +
    tasks.map(fn(t) { t.name() }).join(",") +
    ")"
  new_composed_async_task(
    name,
    tasks,
    async fn(ctx : Context) -> Result[Unit, MulpError] {
      let errors : Array[(String, String)] = []
      @async.with_task_group(group => {
        for task in tasks {
          let t = task
          group.spawn_bg(allow_failure=true, async fn() -> Unit {
            match t.execute(ctx) {
              Ok(_) => ()
              Err(e) => errors.push((t.name(), e.message()))
            }
          })
        }
      })
      if errors.length() > 0 {
        Err(parallel_failed(errors))
      } else {
        Ok(())
      }
    },
  )
}

///|
pub fn AsyncTask::name(self : AsyncTask) -> String {
  self.name
}

///|
pub fn AsyncTask::children(self : AsyncTask) -> Array[AsyncTask] {
  self.children
}

///|
pub async fn AsyncTask::execute(
  self : AsyncTask,
  ctx : Context,
) -> Result[Unit, MulpError] {
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
pub fn AsyncRegistry::tasks_json(self : AsyncRegistry) -> String {
  tasks_json_from_tree(self.tree())
}

///|
pub fn last_run_async(task : AsyncTask) -> Int64? {
  task.state.last_run_ms
}
