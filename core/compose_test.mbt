///|
fn push(trace : Array[String], label : String) -> TaskRun {
  fn(_ctx : Context) -> Result[Unit, MulpError] {
    trace.push(label)
    Ok(())
  }
}

///|
fn fail(label : String) -> TaskRun {
  fn(_ctx : Context) -> Result[Unit, MulpError] {
    Err(task_failed(label, "boom"))
  }
}

///|
test "series runs tasks in order" {
  let trace : Array[String] = []
  let first = new_task("first", push(trace, "first"))
  let second = new_task("second", push(trace, "second"))
  let composed = series([first, second])
  let ctx = new_context(
    cwd="/workspace/app",
    now_ms=2000L,
    cancellation=new_cancellation_token(),
  )
  let result = composed.execute(ctx)
  inspect(result is Ok(_), content="true")
  inspect(trace, content="[\"first\", \"second\"]")
}

///|
test "task execute supports synchronous task completion" {
  let trace : Array[String] = []
  let item = new_task("build", fn(_ctx : Context) -> Result[Unit, MulpError] {
    trace.push("done")
    Ok(())
  })
  let ctx = new_context(
    cwd="/workspace/app",
    now_ms=1800L,
    cancellation=new_cancellation_token(),
  )
  let result = item.execute(ctx)
  inspect(result is Ok(_), content="true")
  inspect(trace, content="[\"done\"]")
  inspect(last_run(item) is Some(1800L), content="true")
}

///|
test "series stops on first failure" {
  let trace : Array[String] = []
  let first = new_task("first", fail("first"))
  let second = new_task("second", push(trace, "second"))
  let composed = series([first, second])
  let ctx = new_context(
    cwd="/workspace/app",
    now_ms=2000L,
    cancellation=new_cancellation_token(),
  )
  let result = composed.execute(ctx)
  inspect(result is Err(_), content="true")
  inspect(result.unwrap_err().message(), content="Task failed: first: boom")
  inspect(trace, content="[]")
}

///|
test "task execute propagates result errors" {
  let item = new_task("build", fn(_ctx : Context) -> Result[Unit, MulpError] {
    Err(task_failed("build", "broken"))
  })
  let ctx = new_context(
    cwd="/workspace/app",
    now_ms=2000L,
    cancellation=new_cancellation_token(),
  )
  let result = item.execute(ctx)
  inspect(result is Err(_), content="true")
  inspect(result.unwrap_err().message(), content="Task failed: build: broken")
  inspect(last_run(item) is None, content="true")
}

///|
test "parallel runs every task when all succeed" {
  let trace : Array[String] = []
  let first = new_task("first", push(trace, "first"))
  let second = new_task("second", push(trace, "second"))
  let composed = parallel([first, second])
  let ctx = new_context(
    cwd="/workspace/app",
    now_ms=3000L,
    cancellation=new_cancellation_token(),
  )
  let result = composed.execute(ctx)
  inspect(result is Ok(_), content="true")
  inspect(trace.length(), content="2")
}

///|
test "parallel reports first failure and requests cancellation" {
  let token = new_cancellation_token()
  let trace : Array[String] = []
  let first = new_task("first", fail("first"))
  let second = new_task("second", push(trace, "second"))
  let composed = parallel([first, second])
  let ctx = new_context(cwd="/workspace/app", now_ms=3000L, cancellation=token)
  let result = composed.execute(ctx)
  inspect(result is Err(_), content="true")
  inspect(result.unwrap_err().message(), content="Task failed: first: boom")
  inspect(token.is_cancelled(), content="true")
}

///|
test "last_run records successful task execution time" {
  let item = new_task("build", fn(_ctx : Context) -> Result[Unit, MulpError] {
    Ok(())
  })
  inspect(last_run(item) is None, content="true")
  let ctx = new_context(
    cwd="/workspace/app",
    now_ms=4000L,
    cancellation=new_cancellation_token(),
  )
  ignore(item.execute(ctx))
  inspect(last_run(item) is Some(4000L), content="true")
}

///|
async test "async_parallel_settle runs all tasks even when one fails" {
  let order : Array[String] = []
  let task_a = new_async_task("a", fn(_ctx : Context) -> Result[Unit, MulpError] {
    order.push("a_start")
    Err(config_error("a failed"))
  })
  let task_b = new_async_task("b", fn(_ctx : Context) -> Result[Unit, MulpError] {
    order.push("b_start")
    Ok(())
  })
  let settled = async_parallel_settle([task_a, task_b])
  let ctx = new_context(
    cwd="/tmp",
    now_ms=0L,
    cancellation=new_cancellation_token(),
  )
  let result = settled.execute(ctx)
  // Both tasks ran
  inspect(order.contains("a_start"), content="true")
  inspect(order.contains("b_start"), content="true")
  // Error is reported
  inspect(result is Err(_), content="true")
}

///|
async test "async_parallel_settle succeeds when all tasks succeed" {
  let task_a = new_async_task("a", fn(_ctx : Context) -> Result[Unit, MulpError] {
    Ok(())
  })
  let task_b = new_async_task("b", fn(_ctx : Context) -> Result[Unit, MulpError] {
    Ok(())
  })
  let settled = async_parallel_settle([task_a, task_b])
  let ctx = new_context(
    cwd="/tmp",
    now_ms=0L,
    cancellation=new_cancellation_token(),
  )
  let result = settled.execute(ctx)
  inspect(result is Ok(_), content="true")
}

///|
test "last_run survives task copies from registry" {
  let registry = new_registry()
  let original = task(registry, "build", fn(_ctx : Context) -> Result[Unit, MulpError] {
    Ok(())
  })
  let found = registry.require("build").unwrap()
  let ctx = new_context(
    cwd="/workspace/app",
    now_ms=5000L,
    cancellation=new_cancellation_token(),
  )
  ignore(found.execute(ctx))
  inspect(last_run(original) is Some(5000L), content="true")
}
