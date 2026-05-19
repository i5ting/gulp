///|
async fn ok_task(_ctx : @core.Context) -> Result[Unit, @core.MulpError] {
  @async.sleep(0)
  Ok(())
}

///|
async test "undertaker registers and retrieves a task by name" {
  let taker = new_undertaker()
  let build = taker.task("build", ok_task).unwrap()
  inspect(build.name(), content="build")
  inspect(taker.get_task("build") is Some(_), content="true")
  inspect(taker.require_task("build").unwrap().name(), content="build")
  inspect(taker.registry().names(), content="[\"build\"]")
}

///|
async test "undertaker rejects duplicate task names" {
  let taker = new_undertaker()
  ignore(taker.task("build", ok_task).unwrap())
  let result = taker.task("build", ok_task)
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Config error: duplicate task: build",
  )
}

///|
async test "undertaker reports missing tasks" {
  let taker = new_undertaker()
  inspect(taker.get_task("missing") is None, content="true")
  let result = taker.require_task("missing")
  inspect(result is Err(_), content="true")
  inspect(result.unwrap_err().message(), content="Task not found: missing")
}

///|
async test "undertaker can wrap a pre-populated registry" {
  let registry = @core.new_async_registry()
  ignore(@core.async_task(registry, "clean", ok_task))
  let taker = new_undertaker_with_registry(registry)
  inspect(taker.registry().names(), content="[\"clean\"]")
  inspect(taker.require_task("clean").unwrap().name(), content="clean")
}

///|
async test "undertaker series resolves named and direct task refs in order" {
  let trace : Array[String] = []
  let taker = new_undertaker()
  ignore(
    taker
    .task("clean", async fn(
      _ctx : @core.Context,
    ) -> Result[Unit, @core.MulpError] {
      @async.sleep(0)
      trace.push("clean")
      Ok(())
    })
    .unwrap(),
  )
  let scripts = @core.new_async_task("scripts", async fn(
    _ctx : @core.Context,
  ) -> Result[Unit, @core.MulpError] {
    @async.sleep(0)
    trace.push("scripts")
    Ok(())
  })
  let build = taker.series([named("clean"), direct(scripts)]).unwrap()
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=9000L,
    cancellation=@core.new_cancellation_token(),
  )
  inspect(build.execute(ctx) is Ok(_), content="true")
  inspect(trace, content="[\"clean\", \"scripts\"]")
}

///|
async test "undertaker series reports missing named refs" {
  let taker = new_undertaker()
  let result = taker.series([named("missing")])
  inspect(result is Err(_), content="true")
  inspect(result.unwrap_err().message(), content="Task not found: missing")
}

///|
async test "undertaker series rejects empty composition" {
  let taker = new_undertaker()
  let result = taker.series([])
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Config error: series requires at least one task",
  )
}

///|
async test "undertaker series stops after first failure" {
  let trace : Array[String] = []
  let taker = new_undertaker()
  ignore(
    taker
    .task("fail", async fn(
      _ctx : @core.Context,
    ) -> Result[Unit, @core.MulpError] {
      @async.sleep(0)
      Err(@core.task_failed("fail", "boom"))
    })
    .unwrap(),
  )
  ignore(
    taker
    .task("later", async fn(
      _ctx : @core.Context,
    ) -> Result[Unit, @core.MulpError] {
      @async.sleep(0)
      trace.push("later")
      Ok(())
    })
    .unwrap(),
  )
  let build = taker.series([named("fail"), named("later")]).unwrap()
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=9010L,
    cancellation=@core.new_cancellation_token(),
  )
  let result = build.execute(ctx)
  inspect(result is Err(_), content="true")
  inspect(result.unwrap_err().message(), content="Task failed: fail: boom")
  inspect(trace, content="[]")
}

///|
async test "undertaker parallel resolves refs and runs concurrently" {
  let trace : Array[String] = []
  let taker = new_undertaker()
  ignore(
    taker
    .task("first", async fn(
      _ctx : @core.Context,
    ) -> Result[Unit, @core.MulpError] {
      trace.push("first:start")
      @async.sleep(20)
      trace.push("first:finish")
      Ok(())
    })
    .unwrap(),
  )
  let second = @core.new_async_task("second", async fn(
    _ctx : @core.Context,
  ) -> Result[Unit, @core.MulpError] {
    trace.push("second:start")
    @async.sleep(0)
    trace.push("second:finish")
    Ok(())
  })
  let build = taker.parallel([named("first"), direct(second)]).unwrap()
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=9020L,
    cancellation=@core.new_cancellation_token(),
  )
  let result = build.execute(ctx)
  inspect(result is Ok(_), content="true")
  inspect(
    trace,
    content="[\"first:start\", \"second:start\", \"second:finish\", \"first:finish\"]",
  )
}

///|
async test "undertaker parallel rejects empty composition" {
  let taker = new_undertaker()
  let result = taker.parallel([])
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Config error: parallel requires at least one task",
  )
}

///|
async test "undertaker parallel failure cancels shared context" {
  let token = @core.new_cancellation_token()
  let taker = new_undertaker()
  ignore(
    taker
    .task("fail", async fn(
      _ctx : @core.Context,
    ) -> Result[Unit, @core.MulpError] {
      @async.sleep(0)
      Err(@core.task_failed("fail", "boom"))
    })
    .unwrap(),
  )
  let build = taker.parallel([named("fail")]).unwrap()
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=9030L,
    cancellation=token,
  )
  let result = build.execute(ctx)
  inspect(result is Err(_), content="true")
  inspect(token.is_cancelled(), content="true")
}

///|
async test "undertaker tree renders shallow and deep task trees" {
  let taker = new_undertaker()
  ignore(taker.task("clean", ok_task).unwrap())
  ignore(taker.task("scripts", ok_task).unwrap())
  let build = taker.series([named("clean"), named("scripts")]).unwrap()
  ignore(taker.task_from("build", build).unwrap())
  inspect(
    taker.tree(deep=false).render(),
    content="root\n  clean\n  scripts\n  build",
  )
  inspect(
    taker.tree(deep=true).render(),
    content="root\n  clean\n  scripts\n  build\n    series\n      clean\n      scripts",
  )
}

///|
async test "undertaker task_from aliases a composed task" {
  let taker = new_undertaker()
  let clean = taker.task("clean", ok_task).unwrap()
  let alias_task = taker.task_from("clean-again", clean).unwrap()
  inspect(alias_task.name(), content="clean-again")
  inspect(
    taker.tree(deep=true).render(),
    content="root\n  clean\n  clean-again\n    clean",
  )
}

///|
async test "undertaker last_run tracks successful task execution" {
  let taker = new_undertaker()
  let build = taker.task("build", ok_task).unwrap()
  inspect(
    taker.last_run("build", precision=None).unwrap() is None,
    content="true",
  )
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=9876L,
    cancellation=@core.new_cancellation_token(),
  )
  ignore(build.execute(ctx))
  inspect(
    taker.last_run("build", precision=None).unwrap() is Some(9876L),
    content="true",
  )
  inspect(
    taker.last_run("build", precision=Some(1000L)).unwrap() is Some(9000L),
    content="true",
  )
}

///|
async test "undertaker last_run rejects missing task and invalid precision" {
  let taker = new_undertaker()
  ignore(taker.task("build", ok_task).unwrap())
  inspect(
    taker.last_run("missing", precision=None).unwrap_err().message(),
    content="Task not found: missing",
  )
  inspect(
    taker.last_run("build", precision=Some(0L)).unwrap_err().message(),
    content="Config error: precision must be positive",
  )
}

///|
async test "undertaker set_registry migrates tasks to new registry" {
  let taker = new_undertaker()
  ignore(taker.task("build", async fn(_ctx) { @async.sleep(0); Ok(()) }).unwrap())
  ignore(taker.task("clean", async fn(_ctx) { @async.sleep(0); Ok(()) }).unwrap())

  let new_reg = @core.new_async_registry_with_name("custom")
  taker.set_registry(new_reg)

  // All tasks migrated
  inspect(taker.registry().names().length(), content="2")
  inspect(taker.registry().find("build") is Some(_), content="true")
  inspect(taker.registry().find("clean") is Some(_), content="true")
}

///|
async test "undertaker set_registry calls init on new registry" {
  let taker = new_undertaker()
  ignore(taker.task("build", async fn(_ctx) { @async.sleep(0); Ok(()) }).unwrap())

  let init_called : Array[String] = []
  let new_reg = @core.new_async_registry_with_init(fn(_reg) {
    init_called.push("init")
  })
  taker.set_registry(new_reg)

  inspect(init_called, content="[\"init\"]")
}

///|
async test "undertaker set_registry replaces old registry" {
  let taker = new_undertaker()
  ignore(taker.task("old_task", async fn(_ctx) { @async.sleep(0); Ok(()) }).unwrap())

  let new_reg = @core.new_async_registry_with_name("new")
  taker.set_registry(new_reg)

  // New registry name is now active
  inspect(taker.registry().name(), content="new")
}

///|
async test "undertaker last_run is not updated after failure" {
  let taker = new_undertaker()
  let fail = taker
    .task("fail", async fn(
      _ctx : @core.Context,
    ) -> Result[Unit, @core.MulpError] {
      @async.sleep(0)
      Err(@core.task_failed("fail", "boom"))
    })
    .unwrap()
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=9900L,
    cancellation=@core.new_cancellation_token(),
  )
  ignore(fail.execute(ctx))
  inspect(
    taker.last_run("fail", precision=None).unwrap() is None,
    content="true",
  )
}
