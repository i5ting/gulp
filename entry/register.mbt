///| Task entry packages expose a function matching this type, conventionally
///| named `register_tasks`, and add their tasks to the provided registry.
pub type RegisterTasks = (@core.Registry) -> Unit

///|
pub fn new_registry() -> @core.Registry {
  @core.new_registry()
}

///|
pub fn task(
  registry : @core.Registry,
  name : String,
  run : @core.TaskRun,
) -> @core.Task {
  @core.task(registry, name, run)
}

///|
pub fn run_args(
  registry : @core.Registry,
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
    None =>
    Err(@core.config_error("missing task argument"))
    Some(name) =>
    match registry.require(name) {
      Ok(found) => found.execute(ctx)
      Err(err) => Err(err)
    }
  }
}

///|
pub fn render_args(
  registry : @core.Registry,
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
pub fn last_run(task : @core.Task) -> Int64? {
  @core.last_run(task)
}
