///|
pub struct AsyncProcessCommand {
  program : String
  args : Array[String]
  cwd : String?
}

///|
pub fn async_process_command(
  program : String,
  args : Array[String],
) -> AsyncProcessCommand {
  { program, args, cwd: None }
}

///|
pub fn AsyncProcessCommand::with_cwd(
  self : AsyncProcessCommand,
  cwd : String,
) -> AsyncProcessCommand {
  { program: self.program, args: self.args, cwd: Some(cwd) }
}

///|
pub fn async_task_from_process(
  name : String,
  command : AsyncProcessCommand,
) -> @core.AsyncTask {
  @core.new_async_task(name, async fn(ctx : @core.Context) -> Result[Unit, @core.MulpError] {
    match @core.validate_async_completion_signals(name, [@core.ProcessCompletion]) {
      Ok(_) => ()
      Err(err) => return Err(err)
    }
    match run_process_async(name, command, ctx) {
      Ok(code) =>
        if code == 0 {
          Ok(())
        } else {
          Err(@core.task_failed(name, "process exited with code \{code}"))
        }
      Err(err) => Err(err)
    }
  })
}

///|
async fn run_process_async(
  task_name : String,
  command : AsyncProcessCommand,
  ctx : @core.Context,
) -> Result[Int, @core.MulpError] {
  @async.with_task_group(group => {
    let process = match command.cwd {
      Some(cwd) =>
        @process.spawn(group, command.program, command.args, cwd=cwd)
      None => @process.spawn(group, command.program, command.args)
    }
    group.spawn_bg(no_wait=true, () => {
      while !ctx.is_cancelled() {
        @async.sleep(10)
      }
      process.cancel()
    })
    let code = process.wait() catch {
      _ if ctx.is_cancelled() => -1
      err => raise err
    }
    if ctx.is_cancelled() {
      Err(@core.task_failed(task_name, "cancelled"))
    } else {
      Ok(code)
    }
  })
}
