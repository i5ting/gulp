///|
pub struct ConfigDocument {
  path : String
  contents : String
}

///|
fn default_config_file_name() -> String {
  "gulp.mbtx"
}

///|
pub fn config_document(path : String, contents : String) -> ConfigDocument {
  { path, contents }
}

///|
fn normalize_dir(path : String) -> String {
  if path != "/" && path.has_suffix("/") {
    path[:path.length() - 1].to_owned()
  } else {
    path
  }
}

///|
fn join_path(dir : String, path : String) -> String {
  if path.has_prefix("/") {
    path
  } else if dir == "/" {
    "/" + path
  } else {
    normalize_dir(dir) + "/" + path
  }
}

///|
fn parent_dir(path : String) -> String? {
  let path = normalize_dir(path)
  if path == "" || path == "/" {
    None
  } else {
    let mut index = path.length() - 1
    while index >= 0 {
      if path[index:index + 1].to_owned() == "/" {
        if index == 0 {
          return Some("/")
        } else {
          return Some(path[:index].to_owned())
        }
      }
      index = index - 1
    }
    None
  }
}

///|
fn find_file(path : String, files : Array[(String, String)]) -> ConfigDocument? {
  for item in files {
    if item.0 == path {
      return Some({ path: item.0, contents: item.1 })
    }
  }
  None
}

///|
pub fn discover_config(
  cwd~ : String,
  files~ : Array[(String, String)],
) -> Result[ConfigDocument, @core.MulpError] {
  let start = normalize_dir(cwd)
  let mut dir = start
  while true {
    let candidate = if dir == "/" {
      "/" + default_config_file_name()
    } else {
      dir + "/" + default_config_file_name()
    }
    match find_file(candidate, files) {
      Some(found) => return Ok(found)
      None =>
        match parent_dir(dir) {
          Some(parent) => dir = parent
          None =>
            return Err(
              @core.config_error(
                "could not find " +
                default_config_file_name() +
                " from " +
                start,
              ),
            )
        }
    }
  }
  Err(
    @core.config_error(
      "could not find " + default_config_file_name() + " from " + start,
    ),
  )
}

///|
pub fn load_config(
  cwd~ : String,
  config_path~ : String?,
  files : Array[(String, String)],
) -> Result[ConfigDocument, @core.MulpError] {
  match config_path {
    Some(path) => {
      let resolved = join_path(cwd, path)
      match find_file(resolved, files) {
        Some(found) => Ok(found)
        None => Err(@core.config_error("could not read config: " + resolved))
      }
    }
    None => discover_config(cwd~, files~)
  }
}
