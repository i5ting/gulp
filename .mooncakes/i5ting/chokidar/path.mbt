///|
fn trim_trailing_slashes(path : String) -> String {
  if path == "/" {
    "/"
  } else {
    path.trim_end(chars="/").to_owned()
  }
}

///|
fn trim_leading_slashes(path : String) -> String {
  path.trim_start(chars="/").to_owned()
}

///|
fn collapse_slashes(path : String) -> String {
  let parts = path.split("/")
  let kept = parts.filter(part => part != "").map(part => part.to_owned())
  if path.has_prefix("/") {
    "/" + kept.join("/")
  } else {
    kept.join("/")
  }
}

///|
fn normalize_path(path : String) -> String {
  let unix = path.replace_all(old="\\", new="/")
  let collapsed = collapse_slashes(unix)
  let trimmed = trim_trailing_slashes(collapsed)
  if trimmed == "" {
    "."
  } else {
    trimmed
  }
}

///|
fn join_path(parent : String, child : String) -> String {
  let parent = trim_trailing_slashes(normalize_path(parent))
  let child = trim_leading_slashes(normalize_path(child))
  if parent == "" || parent == "." {
    if child == "" || child == "." {
      "."
    } else {
      child
    }
  } else if child == "" || child == "." {
    parent
  } else if parent == "/" {
    "/" + child
  } else {
    parent + "/" + child
  }
}

///|
fn path_basename(path : String) -> String {
  let path = normalize_path(path)
  if path == "" {
    "."
  } else if path == "/" {
    "/"
  } else {
    match path.rev_split_once("/") {
      Some((_, base)) => base.to_owned()
      None => path
    }
  }
}

///|
fn path_dirname(path : String) -> String {
  let path = normalize_path(path)
  if path == "/" {
    "/"
  } else {
    match path.rev_split_once("/") {
      Some((dir, _)) => if dir == "" { "/" } else { dir.to_owned() }
      None => "."
    }
  }
}

///|
fn relative_path(root : String, full_path : String) -> String {
  let root = normalize_path(root)
  let full_path = normalize_path(full_path)
  match full_path.strip_prefix(root) {
    Some(path) =>
      if path == "" {
        ""
      } else if root == "/" {
        path.to_owned()
      } else if path.has_prefix("/") {
        path.trim_start(chars="/").to_owned()
      } else {
        full_path
      }
    None => full_path
  }
}

///|
fn event_path_for(path : String, options : Options) -> String {
  match options.cwd {
    Some(cwd) => relative_path(cwd, path)
    None => path
  }
}

///|
fn absolute_path_for(path : String, options : Options) -> String {
  match options.cwd {
    Some(cwd) => join_path(cwd, path)
    None => path
  }
}
