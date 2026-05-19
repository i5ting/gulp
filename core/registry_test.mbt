///|
fn ok_task(_ctx : Context) -> Result[Unit, MulpError] {
  Ok(())
}

///|
test "registry registers and finds task by name" {
  let registry = new_registry()
  let build = task(registry, "build", ok_task)
  let found = registry.find("build")
  inspect(found is Some(_), content="true")
  inspect(found.unwrap().name(), content="build")
  inspect(build.name(), content="build")
}

///|
test "custom registry can carry a name" {
  let registry = new_registry_with_name("site")
  ignore(task(registry, "build", ok_task))
  inspect(registry.name(), content="site")
  inspect(registry.names(), content="[\"build\"]")
}

///|
test "registry reports missing task" {
  let registry = new_registry()
  let result = registry.require("missing")
  inspect(result is Err(_), content="true")
  inspect(result.unwrap_err().message(), content="Task not found: missing")
}

///|
test "registry checked registration rejects duplicate task names" {
  let registry = new_registry()
  ignore(try_task(registry, "build", ok_task).unwrap())
  let result = try_task(registry, "build", ok_task)
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Config error: duplicate task: build",
  )
}

///|
test "registry init runs a registration callback" {
  let registry = init_registry(fn(registry : Registry) -> Unit {
    ignore(task(registry, "build", ok_task))
    ignore(task(registry, "test", ok_task))
  })
  inspect(registry.names(), content="[\"build\", \"test\"]")
}

///|
test "registry lists task names in registration order" {
  let registry = new_registry()
  ignore(task(registry, "clean", ok_task))
  ignore(task(registry, "build", ok_task))
  inspect(registry.names(), content="[\"clean\", \"build\"]")
}

///|
test "registry can distinguish exported and private tasks" {
  let registry = new_registry()
  ignore(task(registry, "build", ok_task))
  ignore(private_task(registry, "internal", ok_task))
  inspect(registry.names(), content="[\"build\", \"internal\"]")
  inspect(registry.exported_names(), content="[\"build\"]")
  inspect(registry.require("internal").unwrap().is_private(), content="true")
}

///|
test "task can carry display name and description metadata" {
  let registry = new_registry()
  let build = task_with_metadata(
    registry,
    "build",
    ok_task,
    display_name=Some("Build site"),
    description=Some("Compile assets"),
  )
  inspect(build.name(), content="build")
  inspect(build.display_name().unwrap(), content="Build site")
  inspect(build.description().unwrap(), content="Compile assets")
  let found = registry.require("build").unwrap()
  inspect(found.display_name().unwrap(), content="Build site")
  inspect(found.description().unwrap(), content="Compile assets")
}

///|
test "AsyncRegistry init callback is called when registry is initialised" {
  let called : Array[String] = []
  let registry = new_async_registry_with_init(fn(_reg) {
    called.push("init")
  })
  // init is not called on construction — caller fires it
  inspect(called, content="[]")
  registry.call_init()
  inspect(called, content="[\"init\"]")
}

///|
test "AsyncRegistry init callback is optional — default registry has no-op init" {
  let registry = new_async_registry()
  // should not panic
  registry.call_init()
}

///|
test "AsyncRegistry all_tasks returns a copy of registered tasks" {
  let registry = new_async_registry()
  ignore(async_task(registry, "build", fn(_ctx) { Ok(()) }))
  ignore(async_task(registry, "clean", fn(_ctx) { Ok(()) }))
  let all = registry.all_tasks()
  inspect(all.length(), content="2")
  inspect(all[0].name(), content="build")
  inspect(all[1].name(), content="clean")
}

///|
test "AsyncRegistry set_task updates existing task and appends new one" {
  let registry = new_async_registry()
  ignore(async_task(registry, "build", fn(_ctx) { Ok(()) }))
  // add new task via set_task
  let clean = new_async_task("clean", fn(_ctx) { Ok(()) })
  registry.set_task(clean)
  inspect(registry.names(), content="[\"build\", \"clean\"]")
  // overwrite existing task via set_task
  let build2 = new_async_task("build", fn(_ctx) { Ok(()) })
  registry.set_task(build2)
  // count stays at 2
  inspect(registry.names(), content="[\"build\", \"clean\"]")
}
