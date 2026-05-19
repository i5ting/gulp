///|
fn normalize_dir(path : String) -> String {
  if path != "/" && path.has_suffix("/") {
    path[:path.length() - 1].to_owned()
  } else {
    path
  }
}

///|
fn default_config_file_name() -> String {
  "mulp.mbtx"
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
fn normalize_path(path : String) -> String {
  let absolute = path.has_prefix("/")
  let parts : Array[String] = []
  for segment_view in path.split("/") {
    let segment = segment_view.to_owned()
    if segment == "" || segment == "." {
      continue
    } else if segment == ".." {
      if parts.length() > 0 {
        ignore(parts.remove(parts.length() - 1))
      }
    } else {
      parts.push(segment)
    }
  }
  let joined = parts.join("/")
  if absolute {
    "/" + joined
  } else {
    joined
  }
}

///|
fn join_config_path(cwd : String, path : String) -> String {
  if path.has_prefix("/") {
    normalize_path(path)
  } else {
    normalize_path(normalize_dir(cwd) + "/" + path)
  }
}

///|
fn read_text_file(path : String) -> Result[String, @core.MulpError] {
  match @stream.file_byte_stream(path, chunk_size=4096) {
    Ok(stream) => stream.read_all(max_bytes=1024 * 1024)
    Err(_) => Err(@core.config_error("could not read config: " + path))
  }
}

///|
pub fn discover_real_config(
  cwd~ : String,
) -> Result[@cli.ConfigDocument, @core.MulpError] {
  let start = normalize_dir(cwd)
  let mut dir = start
  while true {
    let candidate = if dir == "/" {
      "/" + default_config_file_name()
    } else {
      dir + "/" + default_config_file_name()
    }
    match read_text_file(candidate) {
      Ok(contents) => return Ok(@cli.config_document(candidate, contents))
      Err(_) =>
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
pub fn discover_moon_root(cwd~ : String) -> Result[String, @core.MulpError] {
  let start = normalize_dir(cwd)
  let mut dir = start
  while true {
    let candidate = if dir == "/" {
      "/moon.mod.json"
    } else {
      dir + "/moon.mod.json"
    }
    match read_text_file(candidate) {
      Ok(_) => return Ok(dir)
      Err(_) =>
        match parent_dir(dir) {
          Some(parent) => dir = parent
          None =>
            return Err(
              @core.config_error("could not find moon.mod.json from " + start),
            )
        }
    }
  }
  Err(@core.config_error("could not find moon.mod.json from " + start))
}

///|
pub fn load_real_config(
  cwd~ : String,
  config_path~ : String?,
) -> Result[@cli.ConfigDocument, @core.MulpError] {
  match config_path {
    Some(path) => {
      let resolved = join_config_path(cwd, path)
      match read_text_file(resolved) {
        Ok(contents) => Ok(@cli.config_document(resolved, contents))
        Err(_) => Err(@core.config_error("could not read config: " + resolved))
      }
    }
    None => discover_real_config(cwd~)
  }
}
