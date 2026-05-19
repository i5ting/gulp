///|
pub fn src(files : Array[File]) -> FileStream {
  file_stream(files)
}

///|
pub(all) enum WriteMode {
  Overwrite
  Append
}

///|
fn resolve_src_path(cwd : String, path : String) -> String {
  if path.has_prefix("/") {
    path
  } else if cwd.has_suffix("/") {
    cwd + path
  } else {
    cwd + "/" + path
  }
}

///|
pub fn src_paths(
  paths : Array[String],
  cwd~ : String,
  base~ : String,
) -> FileStream {
  src_paths_with_options(paths, cwd~, base~, read=true, buffer=true)
}

///|
pub fn src_paths_with_options(
  paths : Array[String],
  cwd~ : String,
  base~ : String,
  read~ : Bool,
  buffer~ : Bool,
) -> FileStream {
  let files : Array[File] = []
  for path in paths {
    let contents = if read {
      if buffer {
        Text("")
      } else {
        Bytes(byte_stream([]))
      }
    } else {
      Empty
    }
    files.push(file(cwd~, base~, path=resolve_src_path(cwd, path), contents~))
  }
  file_stream(files)
}

///|
fn parse_int64(value : String) -> Int64? {
  let mut result = 0L
  if value == "" {
    return None
  }
  for index in 0..<value.length() {
    let digit = match value[index:index + 1] {
      "0" => 0L
      "1" => 1L
      "2" => 2L
      "3" => 3L
      "4" => 4L
      "5" => 5L
      "6" => 6L
      "7" => 7L
      "8" => 8L
      "9" => 9L
      _ => return None
    }
    result = result * 10L + digit
  }
  Some(result)
}

///|
pub fn src_since(files : Array[File], since~ : Int64?) -> FileStream {
  match since {
    None => file_stream(files)
    Some(last) => {
      let selected : Array[File] = []
      for file in files {
        match file.mtime_ms() {
          Some(value) =>
            match parse_int64(value) {
              Some(mtime) => if mtime > last { selected.push(file) }
              None => ()
            }
          None => ()
        }
      }
      file_stream(selected)
    }
  }
}

///|
fn find_file_by_path(files : Array[File], path : String) -> File? {
  for file in files {
    if file.path == path {
      return Some(file)
    }
  }
  None
}

///|
fn follow_symlink(file : File, target : File) -> File {
  {
    cwd: file.cwd,
    base: file.base,
    path: file.path,
    contents: target.contents,
    metadata: target.metadata,
    source_map: target.source_map(),
  }
}

///|
pub fn src_symlinks(files : Array[File], follow~ : Bool) -> FileStream {
  if !follow {
    file_stream(files)
  } else {
    let selected : Array[File] = []
    for file in files {
      match file.contents {
        Symlink(target) =>
          match find_file_by_path(files, target) {
            Some(found) => selected.push(follow_symlink(file, found))
            None => selected.push(file)
          }
        _ => selected.push(file)
      }
    }
    file_stream(selected)
  }
}

///|
struct MemoryDest {
  written : Array[File]
  contents : Map[String, Bytes]
}

///|
priv struct DestState {
  mut emitted : Bool
}

///|
pub fn memory_dest() -> MemoryDest {
  { written: [], contents: Map::new() }
}

///|
fn join_path(dir : String, rel : String) -> String {
  if dir.has_suffix("/") {
    dir + rel
  } else {
    dir + "/" + rel
  }
}

///|
fn with_path(file : File, path : String) -> File {
  {
    cwd: file.cwd,
    base: file.base,
    path,
    contents: file.contents,
    metadata: file.metadata,
    source_map: file.source_map(),
  }
}

///|
fn with_metadata_value(file : File, key : String, value : String?) -> File {
  match value {
    None => file
    Some(next) => {
      let metadata : Array[(String, String)] = []
      let mut replaced = false
      for item in file.metadata {
        if item.0 == key {
          metadata.push((key, next))
          replaced = true
        } else {
          metadata.push(item)
        }
      }
      if !replaced {
        metadata.push((key, next))
      }
      {
        cwd: file.cwd,
        base: file.base,
        path: file.path,
        contents: file.contents,
        metadata,
        source_map: file.source_map(),
      }
    }
  }
}

///|
fn with_dest_modes(file : File, mode : String?, dir_mode : String?) -> File {
  match file.contents {
    Directory => with_metadata_value(file, "mode", dir_mode)
    _ => with_metadata_value(file, "mode", mode)
  }
}

///|
pub fn MemoryDest::files(self : MemoryDest) -> Array[File] {
  self.written
}

///|
pub fn MemoryDest::contents_for(self : MemoryDest, path : String) -> String? {
  match self.contents.get(path) {
    Some(contents) => Some(@utf8.decode_lossy(contents))
    None => None
  }
}

///|
pub fn MemoryDest::contents_bytes_for(
  self : MemoryDest,
  path : String,
) -> Bytes? {
  self.contents.get(path)
}

///|
fn append_bytes(
  previous : Bytes?,
  contents : Bytes,
  write_mode : WriteMode,
) -> Bytes {
  match write_mode {
    Overwrite => contents
    Append =>
      match previous {
        Some(before) => before + contents
        None => contents
      }
  }
}

///|
fn MemoryDest::write_text_contents(
  self : MemoryDest,
  path : String,
  contents : String,
  write_mode : WriteMode,
) -> Unit {
  let encoded = @utf8.encode(contents)
  match write_mode {
    Overwrite => self.contents.set(path, encoded)
    Append =>
      self.contents.set(
        path,
        append_bytes(self.contents.get(path), encoded, Append),
      )
  }
}

///|
fn MemoryDest::write_byte_contents(
  self : MemoryDest,
  path : String,
  contents : Bytes,
  write_mode : WriteMode,
) -> Unit {
  match write_mode {
    Overwrite => self.contents.set(path, contents)
    Append =>
      self.contents.set(
        path,
        append_bytes(self.contents.get(path), contents, Append),
      )
  }
}

///|
fn record_contents(
  sink : MemoryDest,
  file : File,
  write_mode : WriteMode,
) -> Result[Unit, @core.MulpError] {
  match file.contents {
    Buffer(contents) => {
      sink.write_byte_contents(file.path, contents, write_mode)
      Ok(())
    }
    Text(contents) => {
      sink.write_text_contents(file.path, contents, write_mode)
      Ok(())
    }
    Bytes(stream) =>
      match stream.read_all_bytes(max_bytes=16 * 1024 * 1024) {
        Ok(contents) => {
          sink.write_byte_contents(file.path, contents, write_mode)
          Ok(())
        }
        Err(err) => Err(err)
      }
    Empty | Directory | Symlink(_) => Ok(())
  }
}

///|
pub fn dest(out_dir : String, sink : MemoryDest) -> Transform {
  dest_with_options(out_dir, sink, sourcemaps=false)
}

///|
pub fn dest_with_options(
  out_dir : String,
  sink : MemoryDest,
  sourcemaps~ : Bool,
) -> Transform {
  dest_with_write_mode_and_sourcemaps(
    out_dir,
    sink,
    write_mode=Overwrite,
    sourcemaps~,
  )
}

///|
pub fn dest_with_write_mode(
  out_dir : String,
  sink : MemoryDest,
  write_mode~ : WriteMode,
) -> Transform {
  dest_with_write_mode_and_sourcemaps(
    out_dir,
    sink,
    write_mode~,
    sourcemaps=false,
  )
}

///|
pub fn dest_with_modes(
  out_dir : String,
  sink : MemoryDest,
  mode~ : String?,
  dir_mode~ : String?,
) -> Transform {
  fn(input : File) -> FileStream {
    let output = with_dest_modes(
      with_path(input, join_path(out_dir, input.relative_path())),
      mode,
      dir_mode,
    )
    let state : DestState = { emitted: false }
    file_stream_from_pull(fn() -> Result[File?, @core.MulpError] {
      if state.emitted {
        Ok(None)
      } else {
        match record_contents(sink, output, Overwrite) {
          Ok(_) => {
            state.emitted = true
            sink.written.push(output)
            Ok(Some(output))
          }
          Err(err) => Err(err)
        }
      }
    })
  }
}

///|
pub fn dest_with_write_mode_and_sourcemaps(
  out_dir : String,
  sink : MemoryDest,
  write_mode~ : WriteMode,
  sourcemaps~ : Bool,
) -> Transform {
  fn(input : File) -> FileStream {
    let output = with_path(input, join_path(out_dir, input.relative_path()))
    let state : DestState = { emitted: false }
    file_stream_from_pull(fn() -> Result[File?, @core.MulpError] {
      if state.emitted {
        Ok(None)
      } else {
        match record_contents(sink, output, write_mode) {
          Ok(_) => {
            state.emitted = true
            sink.written.push(output)
            if sourcemaps {
              match output.source_map() {
                Some(map) =>
                  sink.write_text_contents(
                    output.path + ".map",
                    map,
                    write_mode,
                  )
                None => ()
              }
            }
            Ok(Some(output))
          }
          Err(err) => Err(err)
        }
      }
    })
  }
}
