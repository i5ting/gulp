///|
#cfg(all(target="native", not(platform="windows")))
async test "async process task awaits successful child process" {
  let task = async_task_from_process(
    "echo",
    async_process_command("sh", ["-c", "exit 0"]),
  )
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=9000L,
    cancellation=@core.new_cancellation_token(),
  )
  let result = task.execute(ctx)
  inspect(result is Ok(_), content="true")
  inspect(@core.last_run_async(task) is Some(9000L), content="true")
}

///|
#cfg(all(target="native", not(platform="windows")))
async test "async process task reports non zero exit code" {
  let task = async_task_from_process(
    "fail",
    async_process_command("sh", ["-c", "exit 7"]),
  )
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=9100L,
    cancellation=@core.new_cancellation_token(),
  )
  let result = task.execute(ctx)
  inspect(result is Err(_), content="true")
  inspect(result.unwrap_err().message(), content="Task failed: fail: process exited with code 7")
  inspect(@core.last_run_async(task) is None, content="true")
}

///|
#cfg(all(target="native", not(platform="windows")))
async test "async process task cancels running child process" {
  let token = @core.new_cancellation_token()
  let task = async_task_from_process(
    "slow",
    async_process_command("sh", ["-c", "sleep 5"]),
  )
  let ctx = @core.new_context(cwd="/workspace", now_ms=9200L, cancellation=token)
  @async.with_task_group(group => {
    let running = group.spawn(() => task.execute(ctx))
    @async.sleep(50)
    token.cancel()
    let result = running.wait()
    inspect(result is Err(_), content="true")
    inspect(result.unwrap_err().message(), content="Task failed: slow: cancelled")
    inspect(@core.last_run_async(task) is None, content="true")
  })
}

///|
#cfg(all(target="native", not(platform="windows")))
async test "async process tasks run concurrently in parallel composition" {
  let first = async_task_from_process(
    "first",
    async_process_command("sh", ["-c", "sleep 0.1"]),
  )
  let second = async_task_from_process(
    "second",
    async_process_command("sh", ["-c", "sleep 0.1"]),
  )
  let build = @core.async_parallel([first, second])
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=9300L,
    cancellation=@core.new_cancellation_token(),
  )
  let started = @async.now()
  let result = build.execute(ctx)
  let elapsed = @async.now() - started
  inspect(result is Ok(_), content="true")
  inspect(elapsed < 180L, content="true")
}
