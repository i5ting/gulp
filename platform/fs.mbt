///|
pub(all) enum FileKind {
  RegularFile
  Directory
}

///|
pub struct FileStat {
  kind : FileKind
  path : String
  size : Int
}

///|
priv struct FsEntry {
  path : String
  kind : FileKind
  contents : String
}

///|
struct MemoryFileSystem {
  entries : Array[FsEntry]
}

///|
pub fn memory_file_system() -> MemoryFileSystem {
  { entries: [{ path: "/", kind: Directory, contents: "" }] }
}

///|
fn normalize_path(path : String) -> String {
  if path != "/" && path.has_suffix("/") {
    path[:path.length() - 1].to_owned()
  } else {
    path
  }
}

///|
fn parent_path(path : String) -> String {
  let path = normalize_path(path)
  let mut index = path.length() - 1
  while index >= 0 {
    if path[index:index + 1].to_owned() == "/" {
      if index == 0 {
        return "/"
      } else {
        return path[:index].to_owned()
      }
    }
    index = index - 1
  }
  "/"
}

///|
fn MemoryFileSystem::find_index(self : MemoryFileSystem, path : String) -> Int? {
  let path = normalize_path(path)
  for index in 0..<self.entries.length() {
    if self.entries[index].path == path {
      return Some(index)
    }
  }
  None
}

///|
fn MemoryFileSystem::has_directory(self : MemoryFileSystem, path : String) -> Bool {
  match self.find_index(path) {
    Some(index) => self.entries[index].kind is Directory
    None => false
  }
}

///|
fn MemoryFileSystem::upsert(self : MemoryFileSystem, entry : FsEntry) -> Unit {
  match self.find_index(entry.path) {
    Some(index) => ignore(self.entries.remove(index))
    None => ()
  }
  self.entries.push(entry)
}

///|
pub fn MemoryFileSystem::mkdir(
  self : MemoryFileSystem,
  path : String,
) -> Result[Unit, @core.MulpError] {
  let path = normalize_path(path)
  let parent = parent_path(path)
  if path == "/" {
    Ok(())
  } else if !self.has_directory(parent) {
    Err(@core.file_system_error("missing parent directory: " + parent))
  } else {
    self.upsert({ path, kind: Directory, contents: "" })
    Ok(())
  }
}

///|
pub fn MemoryFileSystem::write_file(
  self : MemoryFileSystem,
  path : String,
  contents : String,
) -> Result[Unit, @core.MulpError] {
  let path = normalize_path(path)
  let parent = parent_path(path)
  if !self.has_directory(parent) {
    Err(@core.file_system_error("missing parent directory: " + parent))
  } else {
    self.upsert({ path, kind: RegularFile, contents })
    Ok(())
  }
}

///|
pub fn MemoryFileSystem::read_file(
  self : MemoryFileSystem,
  path : String,
) -> Result[String, @core.MulpError] {
  let path = normalize_path(path)
  match self.find_index(path) {
    Some(index) =>
      if self.entries[index].kind is RegularFile {
        Ok(self.entries[index].contents)
      } else {
        Err(@core.file_system_error("not a file: " + path))
      }
    None => Err(@core.file_system_error("no such path: " + path))
  }
}

///|
pub fn MemoryFileSystem::stat(
  self : MemoryFileSystem,
  path : String,
) -> Result[FileStat, @core.MulpError] {
  let path = normalize_path(path)
  match self.find_index(path) {
    Some(index) => {
      let entry = self.entries[index]
      Ok({ kind: entry.kind, path: entry.path, size: entry.contents.length() })
    }
    None => Err(@core.file_system_error("no such path: " + path))
  }
}

///|
pub fn MemoryFileSystem::delete(
  self : MemoryFileSystem,
  path : String,
) -> Result[Unit, @core.MulpError] {
  let path = normalize_path(path)
  match self.find_index(path) {
    Some(index) => {
      if self.entries[index].kind is Directory {
        let prefix = if path == "/" { "/" } else { path + "/" }
        for entry in self.entries {
          if entry.path != path && entry.path.has_prefix(prefix) {
            return Err(@core.file_system_error("directory not empty: " + path))
          }
        }
      }
      ignore(self.entries.remove(index))
      Ok(())
    }
    None => Err(@core.file_system_error("no such path: " + path))
  }
}
