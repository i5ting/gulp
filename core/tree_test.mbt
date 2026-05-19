///|
fn noop(_ctx : Context) -> Result[Unit, MulpError] {
  Ok(())
}

///|
test "registry tree lists registered tasks" {
  let registry = new_registry()
  ignore(task(registry, "clean", noop))
  ignore(task(registry, "build", noop))
  let node = registry.tree()
  inspect(node.name, content="root")
  inspect(node.children.length(), content="2")
  inspect(node.render(), content="root\n  clean\n  build")
}

///|
test "registry tree renders series and parallel composition" {
  let registry = new_registry()
  let clean = task(registry, "clean", noop)
  let scripts = task(registry, "scripts", noop)
  let styles = task(registry, "styles", noop)
  let assets = parallel([scripts, styles])
  ignore(task_from(registry, "default", series([clean, assets])))
  let expected = #|root
                 #|  clean
                 #|  scripts
                 #|  styles
                 #|  default
                 #|    series
                 #|      clean
                 #|      parallel
                 #|        scripts
                 #|        styles
  inspect(
    registry.tree().render(),
    content=expected,
  )
}

///|
test "tree node public helpers expose name and children" {
  let child = new_tree_node("child", [])
  let root = new_tree_node("root", [child])
  inspect(root.name(), content="root")
  inspect(root.children().length(), content="1")
  inspect(root.children()[0].name(), content="child")
  inspect(root.render(), content="root\n  child")
}

///|
test "tasks_json renders empty registry" {
  let registry = new_registry()
  inspect(registry.tasks_json(), content="{\"label\":\"Tasks\",\"nodes\":[]}")
}

///|
test "tasks_json renders flat task list" {
  let registry = new_registry()
  ignore(task(registry, "build", noop))
  ignore(task(registry, "test", noop))
  inspect(
    registry.tasks_json(),
    content="{\"label\":\"Tasks\",\"nodes\":[{\"label\":\"build\",\"type\":\"task\",\"nodes\":[]},{\"label\":\"test\",\"type\":\"task\",\"nodes\":[]}]}",
  )
}

///|
test "tasks_json renders nested dependencies" {
  let registry = new_registry()
  let build = task(registry, "build", noop)
  ignore(task_from(registry, "default", series([build])))
  let json = registry.tasks_json()
  inspect(json.has_prefix("{\"label\":\"Tasks\""), content="true")
  inspect(json.has_prefix("{\"label\":\"Tasks\",\"nodes\":[{\"label\":\"build\",\"type\":\"task\""), content="true")
  inspect(
    json.has_prefix(
      "{\"label\":\"Tasks\",\"nodes\":[{\"label\":\"build\",\"type\":\"task\",\"nodes\":[]},{\"label\":\"default\",\"type\":\"task\",\"nodes\":[{\"label\":\"series\",\"type\":\"dep\"",
    ),
    content="true",
  )
}
