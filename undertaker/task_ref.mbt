///|
pub enum TaskRef {
  Named(String)
  Direct(@core.AsyncTask)
}

///|
pub fn named(name : String) -> TaskRef {
  Named(name)
}

///|
pub fn direct(task : @core.AsyncTask) -> TaskRef {
  Direct(task)
}
