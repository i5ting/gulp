///|
pub fn run_tasks(
  tasks : Array[(String, @core.TaskRun)],
  args : Array[String],
  default_task~ : String = "build",
) -> Unit {
  let registry = @core.new_registry()
  for item in tasks {
    let (name, run) = item
    ignore(@core.task(registry, name, run))
  }
  run(registry, args, default_task=default_task)
}

///|
pub fn run(
  registry : @core.Registry,
  args : Array[String],
  default_task~ : String = "build",
) -> Unit {
  match render_args(registry, args) {
    Ok(output) => println(output)
    Err(_) => {
      let effective = if args.length() >= 2 { args } else { ["entry", default_task] }
      let ctx = @core.new_context(
        cwd=".",
        now_ms=@env.now().reinterpret_as_int64(),
        cancellation=@core.new_cancellation_token(),
      )
      match run_args(registry, effective, ctx) {
        Ok(_) => ()
        Err(err) => println("error: " + err.message())
      }
    }
  }
}
