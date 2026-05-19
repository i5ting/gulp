///|
async test "moonbit async task group runs sleeping tasks concurrently" {
  let started : Array[String] = []
  let finished : Array[String] = []
  let start_ms = @async.now()
  @async.with_task_group(group => {
    group.spawn_bg(allow_failure=true, () => {
      started.push("a")
      @async.sleep(100)
      finished.push("a")
    })
    group.spawn_bg(allow_failure=true, () => {
      started.push("b")
      @async.sleep(100)
      finished.push("b")
    })
  })
  let elapsed_ms = @async.now() - start_ms
  inspect(started.length(), content="2")
  inspect(finished.length(), content="2")
  inspect(elapsed_ms < 180L, content="true")
}

///|
async test "async task executes and records last run" {
  let trace : Array[String] = []
  let task = new_async_task("build", fn(_ctx : Context) -> Result[Unit, MulpError] {
    trace.push("build")
    Ok(())
  })
  let ctx = new_context(
    cwd="/workspace/app",
    now_ms=7000L,
    cancellation=new_cancellation_token(),
  )
  let result = task.execute(ctx)
  inspect(result is Ok(_), content="true")
  inspect(trace, content="[\"build\"]")
  inspect(last_run_async(task) is Some(7000L), content="true")
}

///|
async test "sync task can be wrapped as async task" {
  let sync = new_task("clean", fn(_ctx : Context) -> Result[Unit, MulpError] {
    Ok(())
  })
  let task = async_task_from_sync(sync)
  let ctx = new_context(
    cwd="/workspace/app",
    now_ms=7100L,
    cancellation=new_cancellation_token(),
  )
  let result = task.execute(ctx)
  inspect(result is Ok(_), content="true")
  inspect(last_run_async(task) is Some(7100L), content="true")
}

///|
async test "async registry registers and requires task by name" {
  let trace : Array[String] = []
  let registry = new_async_registry()
  let build = async_task(registry, "build", async fn(_ctx : Context) -> Result[Unit, MulpError] {
    @async.sleep(0)
    trace.push("build")
    Ok(())
  })
  let ctx = new_context(
    cwd="/workspace/app",
    now_ms=7150L,
    cancellation=new_cancellation_token(),
  )
  let found = registry.require("build").unwrap()
  let result = found.execute(ctx)
  inspect(result is Ok(_), content="true")
  inspect(registry.names(), content="[\"build\"]")
  inspect(trace, content="[\"build\"]")
  inspect(last_run_async(build) is Some(7150L), content="true")
}

///|
async test "async registry renders composed task tree" {
  let registry = new_async_registry()
  let clean = async_task(registry, "clean", fn(_ctx : Context) -> Result[Unit, MulpError] {
    Ok(())
  })
  let scripts = async_task(registry, "scripts", fn(_ctx : Context) -> Result[Unit, MulpError] {
    Ok(())
  })
  ignore(async_task_from(registry, "build", async_series([clean, scripts])))
  inspect(
    registry.tree().render(),
    content="root\n  clean\n  scripts\n  build\n    series\n      clean\n      scripts",
  )
}

///|
async test "async series runs tasks in order" {
  let trace : Array[String] = []
  let first = new_async_task("first", fn(_ctx : Context) -> Result[Unit, MulpError] {
    trace.push("first")
    Ok(())
  })
  let second = new_async_task("second", fn(_ctx : Context) -> Result[Unit, MulpError] {
    trace.push("second")
    Ok(())
  })
  let composed = async_series([first, second])
  let ctx = new_context(
    cwd="/workspace/app",
    now_ms=7200L,
    cancellation=new_cancellation_token(),
  )
  let result = composed.execute(ctx)
  inspect(result is Ok(_), content="true")
  inspect(trace, content="[\"first\", \"second\"]")
}

///|
async test "async series stops on first failure" {
  let trace : Array[String] = []
  let first = new_async_task("first", fn(_ctx : Context) -> Result[Unit, MulpError] {
    Err(task_failed("first", "boom"))
  })
  let second = new_async_task("second", fn(_ctx : Context) -> Result[Unit, MulpError] {
    trace.push("second")
    Ok(())
  })
  let composed = async_series([first, second])
  let ctx = new_context(
    cwd="/workspace/app",
    now_ms=7300L,
    cancellation=new_cancellation_token(),
  )
  let result = composed.execute(ctx)
  inspect(result is Err(_), content="true")
  inspect(result.unwrap_err().message(), content="Task failed: first: boom")
  inspect(trace, content="[]")
}

///|
async test "async parallel starts tasks concurrently" {
  let started : Array[String] = []
  let finished : Array[String] = []
  let first = new_async_task("first", async fn(_ctx : Context) -> Result[Unit, MulpError] {
    started.push("first")
    @async.sleep(100)
    finished.push("first")
    Ok(())
  })
  let second = new_async_task("second", async fn(_ctx : Context) -> Result[Unit, MulpError] {
    started.push("second")
    @async.sleep(100)
    finished.push("second")
    Ok(())
  })
  let composed = async_parallel([first, second])
  let ctx = new_context(
    cwd="/workspace/app",
    now_ms=7400L,
    cancellation=new_cancellation_token(),
  )
  let start_ms = @async.now()
  let result = composed.execute(ctx)
  let elapsed_ms = @async.now() - start_ms
  inspect(result is Ok(_), content="true")
  inspect(started.length(), content="2")
  inspect(finished.length(), content="2")
  inspect(elapsed_ms < 180L, content="true")
}

///|
async test "async parallel cancels token and aggregates failures" {
  let token = new_cancellation_token()
  let first = new_async_task("first", async fn(_ctx : Context) -> Result[Unit, MulpError] {
    @async.sleep(10)
    Err(task_failed("first", "boom"))
  })
  let second = new_async_task("second", async fn(_ctx : Context) -> Result[Unit, MulpError] {
    @async.sleep(10)
    Err(task_failed("second", "broken"))
  })
  let composed = async_parallel([first, second])
  let ctx = new_context(cwd="/workspace/app", now_ms=7500L, cancellation=token)
  let result = composed.execute(ctx)
  inspect(result is Err(_), content="true")
  inspect(token.is_cancelled(), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Parallel failed: first: Task failed: first: boom; second: Task failed: second: broken",
  )
}

///|
async test "callback task completes when callback succeeds" {
  let task = async_task_from_callback("deploy", fn(done : TaskCallback) -> Unit {
    done(Ok(()))
  })
  let ctx = new_context(
    cwd="/workspace/app",
    now_ms=8200L,
    cancellation=new_cancellation_token(),
  )
  let result = task.execute(ctx)
  inspect(result is Ok(_), content="true")
  inspect(last_run_async(task) is Some(8200L), content="true")
}

///|
async test "callback task can complete asynchronously later" {
  let task = async_task_from_callback("deploy", async fn(done : TaskCallback) -> Unit {
    @async.sleep(10)
    done(Ok(()))
  })
  let ctx = new_context(
    cwd="/workspace/app",
    now_ms=8250L,
    cancellation=new_cancellation_token(),
  )
  let result = task.execute(ctx)
  inspect(result is Ok(_), content="true")
  inspect(last_run_async(task) is Some(8250L), content="true")
}

///|
async test "callback task propagates callback failure" {
  let task = async_task_from_callback("deploy", fn(done : TaskCallback) -> Unit {
    done(Err(task_failed("deploy", "boom")))
  })
  let ctx = new_context(
    cwd="/workspace/app",
    now_ms=8300L,
    cancellation=new_cancellation_token(),
  )
  let result = task.execute(ctx)
  inspect(result is Err(_), content="true")
  inspect(result.unwrap_err().message(), content="Task failed: deploy: boom")
  inspect(last_run_async(task) is None, content="true")
}

///|
async test "callback task waits when callback is not called" {
  let task = async_task_from_callback("deploy", fn(_done : TaskCallback) -> Unit {
    ()
  })
  let ctx = new_context(
    cwd="/workspace/app",
    now_ms=8400L,
    cancellation=new_cancellation_token(),
  )
  let result = @async.with_timeout_opt(20, async fn() -> Result[Unit, MulpError] {
    task.execute(ctx)
  })
  inspect(result is None, content="true")
  inspect(last_run_async(task) is None, content="true")
}

///|
async test "callback task rejects duplicate callback completion" {
  let task = async_task_from_callback("deploy", fn(done : TaskCallback) -> Unit {
    done(Ok(()))
    done(Ok(()))
  })
  let ctx = new_context(
    cwd="/workspace/app",
    now_ms=8500L,
    cancellation=new_cancellation_token(),
  )
  let result = task.execute(ctx)
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Task failed: deploy: callback called more than once",
  )
  inspect(last_run_async(task) is None, content="true")
}

///|
async test "async task timeout cancels shared token" {
  let token = new_cancellation_token()
  let slow = new_async_task("slow", async fn(_ctx : Context) -> Result[Unit, MulpError] {
    @async.sleep(50)
    Ok(())
  })
  let task = async_task_with_timeout(slow, timeout_ms=10)
  let ctx = new_context(cwd="/workspace/app", now_ms=8550L, cancellation=token)
  let result = task.execute(ctx)
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Task failed: slow: timed out after 10ms",
  )
  inspect(token.is_cancelled(), content="true")
  inspect(last_run_async(task) is None, content="true")
  inspect(last_run_async(slow) is None, content="true")
}

///|
async test "async task timeout wrapper allows fast completion" {
  let token = new_cancellation_token()
  let fast = new_async_task("fast", async fn(_ctx : Context) -> Result[Unit, MulpError] {
    @async.sleep(0)
    Ok(())
  })
  let task = async_task_with_timeout(fast, timeout_ms=50)
  let ctx = new_context(cwd="/workspace/app", now_ms=8560L, cancellation=token)
  let result = task.execute(ctx)
  inspect(result is Ok(_), content="true")
  inspect(token.is_cancelled(), content="false")
  inspect(last_run_async(task) is Some(8560L), content="true")
  inspect(last_run_async(fast) is Some(8560L), content="true")
}

///|
async test "completion protocol rejects stream and callback conflict" {
  let result = validate_async_completion_signals(
    "build",
    [StreamCompletion, CallbackCompletion],
  )
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Task failed: build: multiple async completion signals: stream, callback",
  )
}

///|
test "async registry tasks_json renders flat task list" {
  let registry = new_async_registry()
  let run : AsyncTaskRun = fn(_ctx : Context) -> Result[Unit, MulpError] { Ok(()) }
  ignore(async_task(registry, "compile", run))
  ignore(async_task(registry, "test", run))
  inspect(
    registry.tasks_json(),
    content="{\"label\":\"Tasks\",\"nodes\":[{\"label\":\"compile\",\"type\":\"task\",\"nodes\":[]},{\"label\":\"test\",\"type\":\"task\",\"nodes\":[]}]}",
  )
}
