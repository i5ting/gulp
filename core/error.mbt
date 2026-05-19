///|
pub enum MulpError {
  TaskNotFound(String)
  TaskFailed(String, String)
  ParallelFailed(Array[(String, String)])
  FileSystemError(String)
  GlobError(String)
  WatchError(String)
  StreamError(String)
  ConfigError(String)
}

///|
pub fn task_not_found(name : String) -> MulpError {
  TaskNotFound(name)
}

///|
pub fn task_failed(name : String, cause : String) -> MulpError {
  TaskFailed(name, cause)
}

///|
pub fn parallel_failed(errors : Array[(String, String)]) -> MulpError {
  ParallelFailed(errors)
}

///|
pub fn file_system_error(detail : String) -> MulpError {
  FileSystemError(detail)
}

///|
pub fn glob_error(detail : String) -> MulpError {
  GlobError(detail)
}

///|
pub fn watch_error(detail : String) -> MulpError {
  WatchError(detail)
}

///|
pub fn stream_error(detail : String) -> MulpError {
  StreamError(detail)
}

///|
pub fn config_error(detail : String) -> MulpError {
  ConfigError(detail)
}

///|
pub fn MulpError::message(self : MulpError) -> String {
  match self {
    TaskNotFound(name) => "Task not found: " + name
    TaskFailed(name, cause) => "Task failed: " + name + ": " + cause
    ParallelFailed(errors) => {
      let parts : Array[String] = []
      for item in errors {
        parts.push(item.0 + ": " + item.1)
      }
      "Parallel failed: " + parts.join("; ")
    }
    FileSystemError(detail) => "File system error: " + detail
    GlobError(detail) => "Glob error: " + detail
    WatchError(detail) => "Watch error: " + detail
    StreamError(detail) => "Stream error: " + detail
    ConfigError(detail) => "Config error: " + detail
  }
}
