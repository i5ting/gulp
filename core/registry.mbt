///|
struct Registry {
  name : String
  tasks : Array[Task]
}

///|
pub fn new_registry() -> Registry {
  new_registry_with_name("default")
}

///|
pub fn new_registry_with_name(name : String) -> Registry {
  { name, tasks: [] }
}

///|
pub fn init_registry(register : (Registry) -> Unit) -> Registry {
  let registry = new_registry()
  register(registry)
  registry
}

///|
pub fn Registry::name(self : Registry) -> String {
  self.name
}

///|
pub fn task(registry : Registry, name : String, run : TaskRun) -> Task {
  let next = new_task(name, run)
  registry.tasks.push(next)
  next
}

///|
pub fn private_task(registry : Registry, name : String, run : TaskRun) -> Task {
  let next = new_private_task(name, run)
  registry.tasks.push(next)
  next
}

///|
pub fn try_task(
  registry : Registry,
  name : String,
  run : TaskRun,
) -> Result[Task, MulpError] {
  match registry.find(name) {
    Some(_) => Err(config_error("duplicate task: " + name))
    None => Ok(task(registry, name, run))
  }
}

///|
pub fn task_with_metadata(
  registry : Registry,
  name : String,
  run : TaskRun,
  display_name~ : String?,
  description~ : String?,
) -> Task {
  let next = new_task_with_metadata(name, run, display_name~, description~)
  registry.tasks.push(next)
  next
}

///|
pub fn task_from(registry : Registry, name : String, source : Task) -> Task {
  let next = new_composed_task(name, [source], fn(ctx : Context) -> Result[Unit, MulpError] {
    source.execute(ctx)
  })
  registry.tasks.push(next)
  next
}

///|
pub fn Registry::find(self : Registry, name : String) -> Task? {
  for item in self.tasks {
    if item.name == name {
      return Some(item)
    }
  }
  None
}

///|
pub fn Registry::require(
  self : Registry,
  name : String,
) -> Result[Task, MulpError] {
  match self.find(name) {
    Some(found) => Ok(found)
    None => Err(task_not_found(name))
  }
}

///|
pub fn Registry::names(self : Registry) -> Array[String] {
  let names : Array[String] = []
  for item in self.tasks {
    names.push(item.name)
  }
  names
}

///|
pub fn Registry::exported_names(self : Registry) -> Array[String] {
  let names : Array[String] = []
  for item in self.tasks {
    if !item.is_private() {
      names.push(item.name)
    }
  }
  names
}
