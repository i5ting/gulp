///|
pub struct Undertaker {
  mut registry : @core.AsyncRegistry
}

///|
pub fn new_undertaker() -> Undertaker {
  { registry: @core.new_async_registry() }
}

///|
pub fn new_undertaker_with_registry(
  registry : @core.AsyncRegistry,
) -> Undertaker {
  { registry, }
}

///|
pub fn Undertaker::registry(self : Undertaker) -> @core.AsyncRegistry {
  self.registry
}

///|
pub fn Undertaker::set_registry(
  self : Undertaker,
  new_reg : @core.AsyncRegistry,
) -> Unit {
  for task in self.registry.all_tasks() {
    new_reg.set_task(task)
  }
  self.registry = new_reg
  self.registry.call_init()
}

///|
pub fn Undertaker::task(
  self : Undertaker,
  name : String,
  run : @core.AsyncTaskRun,
) -> Result[@core.AsyncTask, @core.MulpError] {
  match self.registry.find(name) {
    Some(_) => Err(@core.config_error("duplicate task: " + name))
    None => Ok(@core.async_task(self.registry, name, run))
  }
}

///|
pub fn Undertaker::task_from(
  self : Undertaker,
  name : String,
  source : @core.AsyncTask,
) -> Result[@core.AsyncTask, @core.MulpError] {
  match self.registry.find(name) {
    Some(_) => Err(@core.config_error("duplicate task: " + name))
    None => Ok(@core.async_task_from(self.registry, name, source))
  }
}

///|
pub fn Undertaker::get_task(
  self : Undertaker,
  name : String,
) -> @core.AsyncTask? {
  self.registry.find(name)
}

///|
pub fn Undertaker::require_task(
  self : Undertaker,
  name : String,
) -> Result[@core.AsyncTask, @core.MulpError] {
  self.registry.require(name)
}

///|
fn Undertaker::resolve_refs(
  self : Undertaker,
  items : Array[TaskRef],
) -> Result[Array[@core.AsyncTask], @core.MulpError] {
  let tasks : Array[@core.AsyncTask] = []
  for item in items {
    match item {
      Named(name) =>
        match self.registry.require(name) {
          Ok(task) => tasks.push(task)
          Err(err) => return Err(err)
        }
      Direct(task) => tasks.push(task)
    }
  }
  Ok(tasks)
}

///|
pub fn Undertaker::series(
  self : Undertaker,
  items : Array[TaskRef],
) -> Result[@core.AsyncTask, @core.MulpError] {
  if items.length() == 0 {
    Err(@core.config_error("series requires at least one task"))
  } else {
    match self.resolve_refs(items) {
      Ok(tasks) => Ok(@core.async_series(tasks))
      Err(err) => Err(err)
    }
  }
}

///|
pub fn Undertaker::parallel(
  self : Undertaker,
  items : Array[TaskRef],
) -> Result[@core.AsyncTask, @core.MulpError] {
  if items.length() == 0 {
    Err(@core.config_error("parallel requires at least one task"))
  } else {
    match self.resolve_refs(items) {
      Ok(tasks) => Ok(@core.async_parallel(tasks))
      Err(err) => Err(err)
    }
  }
}
