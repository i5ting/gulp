///|
pub struct SrcOptions {
  cwd : String
  base : String
  read : Bool
  buffer : Bool
  since : Int64?
  resolve_symlinks : Bool
  dot : Bool
  allow_empty : Bool
  remove_bom : Bool
  encoding : SrcEncoding
  sourcemaps : Bool
  chunk_size : Int
}

///|
pub fn src_options(
  cwd~ : String,
  base? : String = cwd,
  read? : Bool = true,
  buffer? : Bool = true,
  since? : Int64? = None,
  resolve_symlinks? : Bool = true,
  dot? : Bool = false,
  allow_empty? : Bool = false,
  remove_bom? : Bool = true,
  encoding? : SrcEncoding = UTF8,
  sourcemaps? : Bool = false,
  chunk_size? : Int = 65536,
) -> SrcOptions {
  {
    cwd,
    base,
    read,
    buffer,
    since,
    resolve_symlinks,
    dot,
    allow_empty,
    remove_bom,
    encoding,
    sourcemaps,
    chunk_size,
  }
}

///|
fn join_path(dir : String, rel : String) -> String {
  if rel == "" {
    dir
  } else if dir.has_suffix("/") {
    dir + rel
  } else {
    dir + "/" + rel
  }
}

///|
fn metadata_value(metadata : Array[(String, String)], key : String) -> String? {
  for item in metadata {
    if item.0 == key {
      return Some(item.1)
    }
  }
  None
}

///|
fn with_metadata_value(
  metadata : Array[(String, String)],
  key : String,
  value : String,
) -> Array[(String, String)] {
  let next : Array[(String, String)] = []
  let mut replaced = false
  for item in metadata {
    if item.0 == key {
      next.push((key, value))
      replaced = true
    } else {
      next.push(item)
    }
  }
  if !replaced {
    next.push((key, value))
  }
  next
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
fn modified_after(metadata : Array[(String, String)], since : Int64?) -> Bool {
  match since {
    None => true
    Some(limit) =>
      match metadata_value(metadata, "mtime_ms") {
        Some(value) =>
          match parse_int64(value) {
            Some(mtime) => {
              let changed = match metadata_value(metadata, "ctime_ms") {
                Some(ctime_value) =>
                  match parse_int64(ctime_value) {
                    Some(ctime) => if ctime > mtime { ctime } else { mtime }
                    None => mtime
                  }
                None => mtime
              }
              changed > limit
            }
            None => false
          }
        None => false
      }
  }
}

///|
fn discovered_paths(root : String) -> Array[String] {
  let paths : Array[String] = []
  let encoded = @utf8.decode_lossy(discover_paths_bytes(@utf8.encode(root)))
  for line in encoded.split("\n") {
    let path = line.to_owned()
    if path != "" {
      paths.push(path)
    }
  }
  paths
}

///|
fn read_symlink_target(path : String) -> String {
  @utf8.decode_lossy(read_symlink_bytes(@utf8.encode(path)))
}

///|
fn target_metadata(
  path : String,
) -> Result[Array[(String, String)], @core.MulpError] {
  let encoded = @utf8.decode_lossy(stat_metadata_bytes(@utf8.encode(path)))
  if encoded == "" {
    Err(@core.file_system_error("cannot stat path: " + path))
  } else {
    let lines : Array[String] = []
    for line in encoded.split("\n") {
      lines.push(line.to_owned())
    }
    if lines.length() < 4 {
      Err(@core.file_system_error("cannot stat path: " + path))
    } else {
      Ok([
        ("kind", lines[0]),
        ("size", lines[1]),
        ("mtime_ms", lines[2]),
        ("mode", lines[3]),
        ("ctime_ms", if lines.length() >= 5 { lines[4] } else { lines[2] }),
      ])
    }
  }
}

///|
fn vinyl_kind_from_metadata(kind : String?) -> @vinyl.VinylKind {
  match kind {
    Some("directory") => @vinyl.Directory
    Some("symlink") => @vinyl.SymbolicLink
    _ => @vinyl.File
  }
}

///|
fn parse_octal_int(value : String) -> Int? {
  let mut result = 0
  if value == "" {
    return None
  }
  for index in 0..<value.length() {
    let digit = match value[index:index + 1] {
      "0" => 0
      "1" => 1
      "2" => 2
      "3" => 3
      "4" => 4
      "5" => 5
      "6" => 6
      "7" => 7
      _ => return None
    }
    result = result * 8 + digit
  }
  Some(result)
}

///|
fn vinyl_stat_from_metadata(
  metadata : Array[(String, String)],
) -> @vinyl.VinylStat {
  @vinyl.vinyl_stat(
    kind=vinyl_kind_from_metadata(metadata_value(metadata, "kind")),
    mode=match metadata_value(metadata, "mode") {
      Some(mode) => parse_octal_int(mode)
      None => None
    },
    mtime_ms=match metadata_value(metadata, "mtime_ms") {
      Some(mtime) => parse_int64(mtime)
      None => None
    },
  )
}

///|
fn vinyl_contents_for(
  path : String,
  kind : String?,
  options : SrcOptions,
) -> Result[@vinyl.VinylContents, @core.MulpError] {
  if !options.read ||
    kind == Some("directory") ||
    (kind == Some("symlink") && !options.resolve_symlinks) {
    Ok(@vinyl.Null)
  } else if options.buffer {
    match @stream.file_byte_stream(path, chunk_size=options.chunk_size) {
      Ok(bytes) =>
        match bytes.read_all_bytes(max_bytes=67108864) {
          Ok(contents) =>
            Ok(
              @vinyl.Buffer(
                decode_src_bytes(
                  contents,
                  options.encoding,
                  remove_bom=options.remove_bom,
                ),
              ),
            )
          Err(err) => Err(err)
        }
      Err(err) => Err(err)
    }
  } else {
    match @stream.file_byte_stream(path, chunk_size=options.chunk_size) {
      Ok(bytes) => Ok(@vinyl.Stream(bytes))
      Err(err) => Err(err)
    }
  }
}

///|
fn source_map_for(path : String, options : SrcOptions) -> String? {
  if !options.sourcemaps {
    None
  } else {
    match
      @stream.file_byte_stream(path + ".map", chunk_size=options.chunk_size) {
      Ok(bytes) =>
        match bytes.read_all(max_bytes=67108864) {
          Ok(source_map) => Some(source_map)
          Err(_) => None
        }
      Err(_) => None
    }
  }
}

///|
fn source_file_for(
  rel : String,
  options : SrcOptions,
) -> Result[@vinyl.VinylFile?, @core.MulpError] {
  let path = join_path(options.cwd, rel)
  match @stream.real_file_metadata(path) {
    Ok(link_metadata) => {
      let metadata = if metadata_value(link_metadata, "kind") == Some("symlink") &&
        options.resolve_symlinks {
        match target_metadata(path) {
          Ok(target) => target
          Err(err) => return Err(err)
        }
      } else {
        link_metadata
      }
      if !modified_after(metadata, options.since) {
        Ok(None)
      } else {
        let kind = metadata_value(metadata, "kind")
        match vinyl_contents_for(path, kind, options) {
          Ok(contents) =>
            Ok(
              Some(
                @vinyl.vinyl_file(
                  cwd=options.cwd,
                  base=options.base,
                  path=Some(path),
                  contents~,
                  stat=Some(vinyl_stat_from_metadata(metadata)),
                  symlink=if kind == Some("symlink") &&
                    !options.resolve_symlinks {
                    Some(read_symlink_target(path))
                  } else {
                    None
                  },
                  source_map=source_map_for(path, options),
                  metadata=if options.read {
                    metadata
                  } else {
                    with_metadata_value(metadata, "vinyl_fs_read", "false")
                  },
                ),
              ),
            )
          Err(err) => Err(err)
        }
      }
    }
    Err(err) => Err(err)
  }
}

fn has_glob_magic(pattern : String) -> Bool {
  for index in 0..<pattern.length() {
    match pattern[index:index + 1] {
      "*" | "?" | "[" | "]" | "{" | "}" => return true
      _ => ()
    }
  }
  false
}

///|
fn validate_globs(
  globs : Array[String],
  options : SrcOptions,
) -> Result[Unit, @core.MulpError] {
  if globs.length() == 0 {
    return Err(@core.task_failed("src", "glob array must not be empty"))
  }
  for glob in globs {
    if glob == "" {
      return Err(@core.task_failed("src", "glob must not be empty"))
    }
    if !glob.has_prefix("!") && !has_glob_magic(glob) {
      match target_metadata(join_path(options.cwd, glob)) {
        Ok(_) => ()
        Err(_) =>
          if options.allow_empty {
            ()
          } else {
            return Err(@core.file_system_error("file not found: " + glob))
          }
      }
    }
  }
  Ok(())
}

///|
pub fn src(globs : Array[String], options : SrcOptions) -> VinylFileStream {
  match validate_globs(globs, options) {
    Ok(_) => ()
    Err(err) => return vinyl_error_stream(err)
  }
  let paths = discovered_paths(options.cwd)
  let selected = select_glob_stream_paths(paths, globs, dot=options.dot)
  let files : Array[@vinyl.VinylFile] = []
  for rel in selected {
    match source_file_for(rel, options) {
      Ok(Some(file)) => files.push(file)
      Ok(None) => ()
      Err(err) => return vinyl_error_stream(err)
    }
  }
  vinyl_file_stream(files)
}

///|
#borrow(root)
extern "c" fn discover_paths_bytes(root : Bytes) -> Bytes = "mulp_vinyl_fs_discover_paths"

///|
#borrow(path)
extern "c" fn read_symlink_bytes(path : Bytes) -> Bytes = "mulp_vinyl_fs_read_symlink"

///|
#borrow(path)
extern "c" fn stat_metadata_bytes(path : Bytes) -> Bytes = "mulp_vinyl_fs_stat_metadata"
