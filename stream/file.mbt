///|
pub(all) enum FileContents {
  Empty
  Text(String)
  Buffer(Bytes)
  Bytes(ByteStream)
  Directory
  Symlink(String)
}

///|
pub struct File {
  cwd : String
  base : String
  path : String
  contents : FileContents
  metadata : Array[(String, String)]
  source_map : String?
}

///|
pub fn file(
  cwd~ : String,
  base~ : String,
  path~ : String,
  contents~ : FileContents,
) -> File {
  { cwd, base, path, contents, metadata: [], source_map: None }
}

///|
pub fn file_with_metadata(
  cwd~ : String,
  base~ : String,
  path~ : String,
  contents~ : FileContents,
  metadata~ : Array[(String, String)],
) -> File {
  { cwd, base, path, contents, metadata, source_map: None }
}

///|
pub fn file_with_source_map(
  cwd~ : String,
  base~ : String,
  path~ : String,
  contents~ : FileContents,
  source_map~ : String?,
) -> File {
  { cwd, base, path, contents, metadata: [], source_map }
}

///|
fn trim_leading_slash(path : String) -> String {
  if path.has_prefix("/") {
    path[1:].to_owned()
  } else {
    path
  }
}

///|
fn path_is_inside_base(path : String, base : String) -> Bool {
  path == base || path.has_prefix(base + "/")
}

///|
fn last_index_of(value : String, needle : String) -> Int? {
  let mut found : Int? = None
  let mut index = 0
  while index < value.length() {
    if value[index:index + 1] == needle {
      found = Some(index)
    }
    index = index + 1
  }
  found
}

///|
pub fn File::relative_path(self : File) -> String {
  if path_is_inside_base(self.path, self.base) {
    let raw = self.path[self.base.length():].to_owned()
    trim_leading_slash(raw)
  } else {
    self.path
  }
}

///|
pub fn File::dirname(self : File) -> String {
  match last_index_of(self.path, "/") {
    Some(0) => "/"
    Some(index) => self.path[:index].to_owned()
    None => "."
  }
}

///|
pub fn File::basename(self : File) -> String {
  match last_index_of(self.path, "/") {
    Some(index) => self.path[index + 1:].to_owned()
    None => self.path
  }
}

///|
pub fn File::extname(self : File) -> String {
  let name = self.basename()
  match last_index_of(name, ".") {
    Some(0) | None => ""
    Some(index) => name[index:].to_owned()
  }
}

///|
pub fn File::stem(self : File) -> String {
  let name = self.basename()
  match last_index_of(name, ".") {
    Some(0) | None => name
    Some(index) => name[:index].to_owned()
  }
}

///|
pub fn File::copy(self : File) -> File {
  {
    cwd: self.cwd,
    base: self.base,
    path: self.path,
    contents: self.contents,
    metadata: self.metadata,
    source_map: self.source_map,
  }
}

///|
pub fn File::with_path(self : File, path : String) -> File {
  {
    cwd: self.cwd,
    base: self.base,
    path,
    contents: self.contents,
    metadata: self.metadata,
    source_map: self.source_map,
  }
}

///|
pub fn File::source_map(self : File) -> String? {
  self.source_map
}

///|
pub fn File::metadata_value(self : File, key : String) -> String? {
  for item in self.metadata {
    if item.0 == key {
      return Some(item.1)
    }
  }
  None
}

///|
pub fn File::kind(self : File) -> String? {
  self.metadata_value("kind")
}

///|
pub fn File::mode(self : File) -> String? {
  self.metadata_value("mode")
}

///|
pub fn File::mtime_ms(self : File) -> String? {
  self.metadata_value("mtime_ms")
}

///|
pub fn File::symlink_target(self : File) -> String? {
  match self.contents {
    Symlink(target) => Some(target)
    _ => None
  }
}
