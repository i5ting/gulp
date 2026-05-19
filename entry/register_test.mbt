///|
fn ok_task(_ctx : @core.Context) -> Result[Unit, @core.MulpError] {
  Ok(())
}

///|
test "entry packages register tasks through a registry callback" {
  let register : RegisterTasks = fn(registry) {
    ignore(@core.task(registry, "build", ok_task))
  }
  let registry = @core.new_registry()
  register(registry)
  inspect(registry.names(), content="[\"build\"]")
}

///|
test "entry helper creates an empty registry" {
  let registry = new_registry()
  inspect(registry.names(), content="[]")
}

///|
test "entry helper registers a named task" {
  let registry = new_registry()
  ignore(task(registry, "test", ok_task))
  inspect(registry.names(), content="[\"test\"]")
}

///|
async test "entry async helper runs the task named by moon run arguments" {
  let trace : Array[String] = []
  let registry = new_async_registry()
  let build = async_task(registry, "build", async fn(_ctx : @core.Context) -> Result[Unit, @core.MulpError] {
    @async.sleep(0)
    trace.push("build")
    Ok(())
  })
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=8100L,
    cancellation=@core.new_cancellation_token(),
  )
  async_run_args(registry, ["entry", "build"], ctx).unwrap()
  inspect(trace, content="[\"build\"]")
  inspect(@core.last_run_async(build) is Some(8100L), content="true")
}

///|
async test "entry async helper runs watch task argument once" {
  let registry = new_async_registry()
  let build = async_task(registry, "build", fn(_ctx : @core.Context) -> Result[Unit, @core.MulpError] {
    Ok(())
  })
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=8200L,
    cancellation=@core.new_cancellation_token(),
  )
  async_run_args(registry, ["entry", "--watch", "build"], ctx).unwrap()
  inspect(@core.last_run_async(build) is Some(8200L), content="true")
}

///|
test "entry async helper renders task names from tasks arguments" {
  let registry = new_async_registry()
  ignore(async_task(registry, "build", fn(_ctx : @core.Context) -> Result[Unit, @core.MulpError] {
    Ok(())
  }))
  ignore(async_task(registry, "test", fn(_ctx : @core.Context) -> Result[Unit, @core.MulpError] {
    Ok(())
  }))
  inspect(render_async_args(registry, ["entry", "--tasks"]).unwrap(), content="build\ntest")
}

///|
test "entry async helper renders task tree from tree arguments" {
  let registry = new_async_registry()
  let clean = async_task(registry, "clean", fn(_ctx : @core.Context) -> Result[Unit, @core.MulpError] {
    Ok(())
  })
  let scripts = async_task(registry, "scripts", fn(_ctx : @core.Context) -> Result[Unit, @core.MulpError] {
    Ok(())
  })
  ignore(async_task_from(registry, "build", @core.async_series([clean, scripts])))
  inspect(
    render_async_args(registry, ["entry", "--tree", "build"]).unwrap(),
    content="root\n  clean\n  scripts\n  build\n    series\n      clean\n      scripts",
  )
}

///|
async test "entry async entry helper renders or runs from script args" {
  let output : Array[String] = []
  let trace : Array[String] = []
  let registry = new_async_registry()
  ignore(async_task(registry, "build", fn(_ctx : @core.Context) -> Result[Unit, @core.MulpError] {
    trace.push("build")
    Ok(())
  }))
  ignore(async_task(registry, "clean", fn(_ctx : @core.Context) -> Result[Unit, @core.MulpError] {
    trace.push("clean")
    Ok(())
  }))
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=8250L,
    cancellation=@core.new_cancellation_token(),
  )
  async_run_entry_args(registry, ["entry", "--tasks"], ctx, fn(text : String) -> Unit {
    output.push(text)
  }).unwrap()
  async_run_entry_args(registry, ["entry"], ctx, fn(text : String) -> Unit {
    output.push(text)
  }).unwrap()
  inspect(output, content="[\"build\\nclean\"]")
  inspect(trace, content="[\"build\"]")
}

///|
async test "entry parallel helper runs async tasks concurrently" {
  let first = @core.new_async_task("first", async fn(_ctx : @core.Context) -> Result[Unit, @core.MulpError] {
    @async.sleep(100)
    Ok(())
  })
  let second = @core.new_async_task("second", async fn(_ctx : @core.Context) -> Result[Unit, @core.MulpError] {
    @async.sleep(100)
    Ok(())
  })
  let build = parallel([first, second])
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=8300L,
    cancellation=@core.new_cancellation_token(),
  )
  let started_ms = @async.now()
  let result = build.execute(ctx)
  let elapsed_ms = @async.now() - started_ms
  inspect(result is Ok(_), content="true")
  inspect(elapsed_ms < 180L, content="true")
}

///|
async test "entry series helper keeps async task order" {
  let trace : Array[String] = []
  let first = @core.new_async_task("first", fn(_ctx : @core.Context) -> Result[Unit, @core.MulpError] {
    trace.push("first")
    Ok(())
  })
  let second = @core.new_async_task("second", fn(_ctx : @core.Context) -> Result[Unit, @core.MulpError] {
    trace.push("second")
    Ok(())
  })
  let build = series([first, second])
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=8400L,
    cancellation=@core.new_cancellation_token(),
  )
  ignore(build.execute(ctx))
  inspect(trace, content="[\"first\", \"second\"]")
}

///|
test "entry helper runs the task named by moon run arguments" {
  let registry = new_registry()
  let build = task(registry, "build", ok_task)
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=7000L,
    cancellation=@core.new_cancellation_token(),
  )
  run_args(registry, ["entry", "build"], ctx).unwrap()
  inspect(@core.last_run(build) is Some(7000L), content="true")
}

///|
test "entry helper runs watch task argument once" {
  let registry = new_registry()
  let build = task(registry, "build", ok_task)
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=8000L,
    cancellation=@core.new_cancellation_token(),
  )
  run_args(registry, ["entry", "--watch", "build"], ctx).unwrap()
  inspect(@core.last_run(build) is Some(8000L), content="true")
}

///|
test "entry helper renders task names from tasks arguments" {
  let registry = new_registry()
  ignore(task(registry, "build", ok_task))
  ignore(task(registry, "test", ok_task))
  inspect(render_args(registry, ["entry", "--tasks"]).unwrap(), content="build\ntest")
}

///|
test "entry helper renders tasks json from tasks-json argument" {
  let registry = new_registry()
  ignore(task(registry, "build", ok_task))
  ignore(task(registry, "test", ok_task))
  inspect(
    render_args(registry, ["entry", "--tasks-json"]).unwrap().has_prefix("{\"label\":\"Tasks\""),
    content="true",
  )
}

///|
test "entry async helper renders tasks json from tasks-json argument" {
  let registry = new_async_registry()
  ignore(async_task(registry, "build", fn(_ctx : @core.Context) -> Result[Unit, @core.MulpError] {
    Ok(())
  }))
  inspect(
    render_async_args(registry, ["entry", "--tasks-json"]).unwrap().has_prefix("{\"label\":\"Tasks\""),
    content="true",
  )
}

///|
test "entry helper renders task tree from tree arguments" {
  let registry = new_registry()
  ignore(task(registry, "build", ok_task))
  ignore(task(registry, "test", ok_task))
  inspect(
    render_args(registry, ["entry", "--tree", "build"]).unwrap(),
    content="root\n  build\n  test",
  )
}

///|
async test "series_by_name resolves tasks and runs them in order" {
  let registry = new_async_registry()
  let order : Array[String] = []
  let _ = async_task(registry, "a", fn(_ctx) {
    order.push("a")
    Ok(())
  })
  let _ = async_task(registry, "b", fn(_ctx) {
    order.push("b")
    Ok(())
  })
  let task = match series_by_name(registry, ["a", "b"]) {
    Ok(t)  => t
    Err(e) => {
      inspect("unexpected error: " + e.message(), content="no error expected")
      return
    }
  }
  let ctx = @core.new_context(
    cwd=".",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  ignore(task.execute(ctx))
  inspect(order, content="[\"a\", \"b\"]")
}

///|
async test "parallel_by_name resolves tasks and runs them" {
  let registry = new_async_registry()
  let ran : Array[String] = []
  let _ = async_task(registry, "x", fn(_ctx) {
    ran.push("x")
    Ok(())
  })
  let _ = async_task(registry, "y", fn(_ctx) {
    ran.push("y")
    Ok(())
  })
  let task = match parallel_by_name(registry, ["x", "y"]) {
    Ok(t)  => t
    Err(e) => {
      inspect("unexpected error: " + e.message(), content="no error expected")
      return
    }
  }
  let ctx = @core.new_context(
    cwd=".",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  ignore(task.execute(ctx))
  inspect(ran.length(), content="2")
}

///|
test "series_by_name returns error for unknown task name" {
  let registry = new_async_registry()
  match series_by_name(registry, ["does-not-exist"]) {
    Ok(_)  => inspect("should have failed", content="expected error")
    Err(e) => inspect(e.message().has_prefix("Task not found"), content="true")
  }
}

///|
test "parallel_by_name returns error for unknown task name" {
  let registry = new_async_registry()
  match parallel_by_name(registry, ["does-not-exist"]) {
    Ok(_)  => inspect("should have failed", content="expected error")
    Err(e) => inspect(e.message().has_prefix("Task not found"), content="true")
  }
}

///|
test "run executes the named task from args" {
  let trace : Array[String] = []
  let registry = new_registry()
  ignore(task(registry, "build", fn(_ctx) {
    trace.push("build")
    Ok(())
  }))
  run(registry, ["entry", "build"])
  inspect(trace, content="[\"build\"]")
}

///|
test "run uses default_task when no task arg is provided" {
  let trace : Array[String] = []
  let registry = new_registry()
  ignore(task(registry, "copy", fn(_ctx) {
    trace.push("copy")
    Ok(())
  }))
  run(registry, ["entry"], default_task="copy")
  inspect(trace, content="[\"copy\"]")
}

///|
test "run does not execute any task for --tasks" {
  let trace : Array[String] = []
  let registry = new_registry()
  ignore(task(registry, "build", fn(_ctx) {
    trace.push("build")
    Ok(())
  }))
  run(registry, ["entry", "--tasks"])
  inspect(trace, content="[]")
}

///|
test "run does not panic for an unknown task name" {
  let registry = new_registry()
  run(registry, ["entry", "does-not-exist"])
}

///|
test "last_run returns None before task executes" {
  let registry = new_registry()
  let t = task(registry, "clean", ok_task)
  inspect(last_run(t) is None, content="true")
}

///|
test "last_run returns Some(now_ms) after task executes" {
  let registry = new_registry()
  let t = task(registry, "stamp", ok_task)
  let ctx = @core.new_context(
    cwd=".",
    now_ms=9000L,
    cancellation=@core.new_cancellation_token(),
  )
  ignore(t.execute(ctx))
  inspect(last_run(t) is Some(9000L), content="true")
}

///|
async test "last_run_async returns None before async task executes" {
  let registry = new_async_registry()
  let t = async_task(registry, "clean", async fn(_ctx) { Ok(()) })
  inspect(last_run_async(t) is None, content="true")
}

///|
async test "last_run_async returns Some(now_ms) after async task executes" {
  let registry = new_async_registry()
  let t = async_task(registry, "stamp", async fn(_ctx) {
    @async.sleep(0)
    Ok(())
  })
  let ctx = @core.new_context(
    cwd=".",
    now_ms=9500L,
    cancellation=@core.new_cancellation_token(),
  )
  ignore(t.execute(ctx))
  inspect(last_run_async(t) is Some(9500L), content="true")
}

///|
test "run_tasks registers and runs a named task" {
  let trace : Array[String] = []
  run_tasks(
    [
      ("build", fn(_ctx) { trace.push("build"); Ok(()) }),
      ("clean", fn(_ctx) { trace.push("clean"); Ok(()) }),
    ],
    ["entry", "build"],
  )
  inspect(trace, content="[\"build\"]")
}

///|
test "run_tasks uses default_task when no arg is provided" {
  let trace : Array[String] = []
  run_tasks(
    [("deploy", fn(_ctx) { trace.push("deploy"); Ok(()) })],
    ["entry"],
    default_task="deploy",
  )
  inspect(trace, content="[\"deploy\"]")
}

///|
test "run_tasks renders task list for --tasks" {
  let registry = new_registry()
  ignore(task(registry, "a", ok_task))
  ignore(task(registry, "b", ok_task))
  inspect(render_args(registry, ["entry", "--tasks"]).unwrap(), content="a\nb")
}
