///| Task entry packages can use this registry when tasks need native async

///|
/// execution, including gulp-style parallel composition.
pub type RegisterAsyncTasks = (@core.AsyncRegistry) -> Unit

///|
pub fn new_async_registry() -> @core.AsyncRegistry {
  @core.new_async_registry()
}

///|
pub fn async_task(
  registry : @core.AsyncRegistry,
  name : String,
  run : @core.AsyncTaskRun,
) -> @core.AsyncTask {
  @core.async_task(registry, name, run)
}

///|
pub fn async_task_from(
  registry : @core.AsyncRegistry,
  name : String,
  source : @core.AsyncTask,
) -> @core.AsyncTask {
  @core.async_task_from(registry, name, source)
}

///|
pub fn series(tasks : Array[@core.AsyncTask]) -> @core.AsyncTask {
  @core.async_series(tasks)
}

///|
pub fn parallel(tasks : Array[@core.AsyncTask]) -> @core.AsyncTask {
  @core.async_parallel(tasks)
}

///|
/// Look up tasks by name in `registry` and compose them in series.
/// Returns Err if any name is not registered.
///
/// Example:
///   let deploy = series_by_name(registry, ["compile", "test", "publish"])!
pub fn series_by_name(
  registry : @core.AsyncRegistry,
  names    : Array[String],
) -> Result[@core.AsyncTask, @core.MulpError] {
  let tasks : Array[@core.AsyncTask] = []
  for name in names {
    match registry.require(name) {
      Ok(task) => tasks.push(task)
      Err(e)   => return Err(e)
    }
  }
  Ok(@core.async_series(tasks))
}

///|
/// Look up tasks by name in `registry` and compose them in parallel.
/// Returns Err if any name is not registered.
///
/// Example:
///   let fetch_all = parallel_by_name(registry, ["fetch-js", "fetch-css"])!
pub fn parallel_by_name(
  registry : @core.AsyncRegistry,
  names    : Array[String],
) -> Result[@core.AsyncTask, @core.MulpError] {
  let tasks : Array[@core.AsyncTask] = []
  for name in names {
    match registry.require(name) {
      Ok(task) => tasks.push(task)
      Err(e)   => return Err(e)
    }
  }
  Ok(@core.async_parallel(tasks))
}

///|
pub async fn async_run_args(
  registry : @core.AsyncRegistry,
  args : Array[String],
  ctx : @core.Context,
) -> Result[Unit, @core.MulpError] {
  let task_name = if args.length() >= 3 && args[1] == "--watch" {
    Some(args[2])
  } else if args.length() >= 2 {
    Some(args[1])
  } else {
    None
  }
  match task_name {
    None => Err(@core.config_error("missing task argument"))
    Some(name) =>
      match registry.require(name) {
        Ok(found) => found.execute(ctx)
        Err(err) => Err(err)
      }
  }
}

///|
pub fn render_async_args(
  registry : @core.AsyncRegistry,
  args : Array[String],
) -> Result[String, @core.MulpError] {
  if args.length() >= 2 && args[1] == "--tasks" {
    Ok(registry.names().join("\n"))
  } else if args.length() >= 2 && args[1] == "--tasks-json" {
    Ok(registry.tasks_json())
  } else if args.length() >= 2 && args[1] == "--tree" {
    Ok(registry.tree().render())
  } else {
    Err(@core.config_error("not a render argument"))
  }
}

///|
pub async fn async_run_entry_args(
  registry : @core.AsyncRegistry,
  args : Array[String],
  ctx : @core.Context,
  write : (String) -> Unit,
  default_task? : String = "build",
) -> Result[Unit, @core.MulpError] {
  match render_async_args(registry, args) {
    Ok(output) => {
      write(output)
      Ok(())
    }
    Err(_) => {
      let run_args = if args.length() >= 2 {
        args
      } else {
        ["entry", default_task]
      }
      async_run_args(registry, run_args, ctx)
    }
  }
}

///|
pub fn last_run_async(task : @core.AsyncTask) -> Int64? {
  @core.last_run_async(task)
}
