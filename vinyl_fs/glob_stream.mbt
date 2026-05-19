///|
fn contains_path(paths : Array[String], path : String) -> Bool {
  for item in paths {
    if item == path {
      return true
    }
  }
  false
}

///|
fn path_has_dot_segment(path : String) -> Bool {
  for segment in path.split("/") {
    let value = segment.to_owned()
    if value.has_prefix(".") {
      return true
    }
  }
  false
}

///|
fn path_matches_glob_option(
  path : String,
  pattern : String,
  dot : Bool,
) -> Bool {
  (dot || !path_has_dot_segment(path)) && @platform.glob_matches(pattern, path)
}

///|
fn remove_matching(
  selected : Array[String],
  pattern : String,
  dot : Bool,
) -> Array[String] {
  let next : Array[String] = []
  for path in selected {
    if !path_matches_glob_option(path, pattern, dot) {
      next.push(path)
    }
  }
  next
}

///|
fn ordered_glob_paths(
  paths : Array[String],
  globs : Array[String],
  dot : Bool,
) -> Array[String] {
  let mut selected : Array[String] = []
  for glob in globs {
    if glob.has_prefix("!") {
      selected = remove_matching(selected, glob[1:].to_owned(), dot)
    } else {
      for path in paths {
        if path_matches_glob_option(path, glob, dot) &&
          !contains_path(selected, path) {
          selected.push(path)
        }
      }
    }
  }
  selected
}

///|
pub fn select_glob_stream_paths(
  paths : Array[String],
  globs : Array[String],
  dot~ : Bool,
) -> Array[String] {
  ordered_glob_paths(paths, globs, dot)
}
