///|
pub type PullVinylFile = () -> Result[@vinyl.VinylFile?, @core.MulpError]

///|
pub type CloseVinylFileStream = () -> Unit

///|
pub type VinylTransform = (@vinyl.VinylFile) -> VinylFileStream

///|
pub type VinylStreamTransform = (VinylFileStream) -> VinylFileStream

///|
struct VinylFileStream {
  pull : PullVinylFile
  close : CloseVinylFileStream
  mut closed : Bool
}

///|
priv struct VinylArrayState {
  files : Array[@vinyl.VinylFile]
  mut index : Int
}

///|
priv struct VinylPipeState {
  upstream : VinylFileStream
  transform : VinylTransform
  mut current : VinylFileStream?
}

///|
priv struct DestState {
  files : Array[@vinyl.VinylFile]
  mut index : Int
}

///|
pub struct DestOptions {
  cwd : String
  mode : Int?
  dir_mode : Int?
  overwrite : Bool
  append : Bool
  sourcemaps : String?
  inline_sourcemaps : Bool
  relative_symlinks : Bool
  encoding : DestEncoding
}

///|
pub fn dest_options(
  cwd? : String = ".",
  mode? : Int? = None,
  dir_mode? : Int? = None,
  overwrite? : Bool = true,
  append? : Bool = false,
  sourcemaps? : String? = None,
  inline_sourcemaps? : Bool = false,
  relative_symlinks? : Bool = false,
  encoding? : DestEncoding = UTF8,
) -> DestOptions {
  {
    cwd,
    mode,
    dir_mode,
    overwrite,
    append,
    sourcemaps,
    inline_sourcemaps,
    relative_symlinks,
    encoding,
  }
}

///|
pub fn vinyl_file_stream(files : Array[@vinyl.VinylFile]) -> VinylFileStream {
  let state : VinylArrayState = { files, index: 0 }
  vinyl_file_stream_from_pull(fn() -> Result[@vinyl.VinylFile?, @core.MulpError] {
    if state.index >= state.files.length() {
      Ok(None)
    } else {
      let file = state.files[state.index]
      state.index = state.index + 1
      Ok(Some(file))
    }
  })
}

///|
pub fn vinyl_file_stream_from_pull(pull : PullVinylFile) -> VinylFileStream {
  vinyl_file_stream_from_pull_with_close(pull, fn() -> Unit { () })
}

///|
pub fn vinyl_file_stream_from_pull_with_close(
  pull : PullVinylFile,
  close : CloseVinylFileStream,
) -> VinylFileStream {
  { pull, close, closed: false }
}

///|
pub fn vinyl_error_stream(err : @core.MulpError) -> VinylFileStream {
  vinyl_file_stream_from_pull(fn() -> Result[@vinyl.VinylFile?, @core.MulpError] {
    Err(err)
  })
}

///|
pub fn VinylFileStream::next(
  self : VinylFileStream,
) -> Result[@vinyl.VinylFile?, @core.MulpError] {
  if self.closed {
    Ok(None)
  } else {
    match (self.pull)() {
      Ok(None) => {
        self.close()
        Ok(None)
      }
      Ok(file) => Ok(file)
      Err(err) => {
        self.close()
        Err(err)
      }
    }
  }
}

///|
pub fn VinylFileStream::close(self : VinylFileStream) -> Unit {
  if !self.closed {
    self.closed = true
    (self.close)()
  }
}

///|
pub fn VinylFileStream::pipe(
  self : VinylFileStream,
  transform : VinylTransform,
) -> VinylFileStream {
  let state : VinylPipeState = { upstream: self, transform, current: None }
  vinyl_file_stream_from_pull_with_close(
    fn() -> Result[@vinyl.VinylFile?, @core.MulpError] {
      while true {
        match state.current {
          Some(current) =>
            match current.next() {
              Ok(Some(file)) => return Ok(Some(file))
              Ok(None) => {
                current.close()
                state.current = None
              }
              Err(err) => return Err(err)
            }
          None =>
            match state.upstream.next() {
              Ok(Some(file)) => state.current = Some((state.transform)(file))
              Ok(None) => return Ok(None)
              Err(err) => return Err(err)
            }
        }
      }
      Ok(None)
    },
    fn() -> Unit {
      match state.current {
        Some(current) => current.close()
        None => ()
      }
      state.upstream.close()
    },
  )
}

///|
pub fn VinylFileStream::pipe_stream(
  self : VinylFileStream,
  transform : VinylStreamTransform,
) -> VinylFileStream {
  transform(self)
}

///|
pub fn VinylFileStream::collect(
  self : VinylFileStream,
  ctx : @core.Context,
) -> Result[Array[@vinyl.VinylFile], @core.MulpError] {
  let files : Array[@vinyl.VinylFile] = []
  while true {
    if ctx.is_cancelled() {
      self.close()
      return Err(@core.task_failed("stream", "cancelled"))
    }
    match self.next() {
      Ok(Some(file)) => files.push(file)
      Ok(None) => return Ok(files)
      Err(err) => {
        self.close()
        return Err(err)
      }
    }
  }
  Ok(files)
}

///|
fn join_dest_path(dir : String, rel : String) -> String {
  if rel == "" {
    dir
  } else if dir.has_suffix("/") {
    dir + rel
  } else {
    dir + "/" + rel
  }
}

///|
fn resolve_dest_dir(out_dir : String, cwd : String) -> String {
  if out_dir.has_prefix("/") {
    out_dir
  } else {
    join_dest_path(cwd, out_dir)
  }
}

///|
fn with_dest_file(
  input : @vinyl.VinylFile,
  options : DestOptions,
  base : String,
) -> @vinyl.VinylFile {
  let stat = dest_stat(input, options)
  let metadata = dest_metadata(input, stat)
  @vinyl.vinyl_file(
    cwd=options.cwd,
    base~,
    path=Some(join_dest_path(base, input.relative())),
    contents=input.contents(),
    stat~,
    symlink=input.symlink(),
    source_map=input.source_map(),
    metadata~,
  )
}

///|
fn dest_stat(
  input : @vinyl.VinylFile,
  options : DestOptions,
) -> @vinyl.VinylStat? {
  let kind = if input.is_directory() {
    @vinyl.Directory
  } else if input.is_symbolic() {
    @vinyl.SymbolicLink
  } else {
    @vinyl.File
  }
  let mode = if input.is_directory() {
    match options.dir_mode {
      Some(value) => Some(value)
      None =>
        match input.stat() {
          Some(stat) => stat.mode()
          None => options.mode
        }
    }
  } else {
    match options.mode {
      Some(value) => Some(value)
      None =>
        match input.stat() {
          Some(stat) => stat.mode()
          None => None
        }
    }
  }
  let mtime_ms = match input.stat() {
    Some(stat) => stat.mtime_ms()
    None => None
  }
  Some(@vinyl.vinyl_stat(kind~, mode~, mtime_ms~))
}

///|
fn dest_metadata(
  input : @vinyl.VinylFile,
  stat : @vinyl.VinylStat?,
) -> Array[(String, String)] {
  match stat {
    Some(value) =>
      match value.mode() {
        Some(mode) =>
          with_metadata_value(input.metadata(), "mode", mode.to_string())
        None => input.metadata()
      }
    None => input.metadata()
  }
}

///|
fn stream_contents_from_vinyl(file : @vinyl.VinylFile) -> @stream.FileContents {
  if file.is_directory() {
    Directory
  } else if file.is_symbolic() {
    match file.symlink() {
      Some(target) => Symlink(target)
      None => Empty
    }
  } else {
    match file.contents() {
      @vinyl.Null => Empty
      @vinyl.Buffer(bytes) => Buffer(bytes)
      @vinyl.Stream(stream) => Bytes(stream)
    }
  }
}

///|
pub fn stream_file_from_vinyl(file : @vinyl.VinylFile) -> @stream.File {
  @stream.file_with_metadata(
    cwd=file.cwd(),
    base=file.base(),
    path=file.path().unwrap_or(""),
    contents=stream_contents_from_vinyl(file),
    metadata=file.metadata(),
  )
}

///|
pub fn vinyl_file_from_stream(file : @stream.File) -> @vinyl.VinylFile {
  let stat = match file.kind() {
    Some("directory") => Some(@vinyl.vinyl_stat(kind=@vinyl.Directory))
    Some("symlink") => Some(@vinyl.vinyl_stat(kind=@vinyl.SymbolicLink))
    Some(_) => Some(@vinyl.vinyl_stat(kind=@vinyl.File))
    None =>
      match file.contents {
        Directory => Some(@vinyl.vinyl_stat(kind=@vinyl.Directory))
        Symlink(_) => Some(@vinyl.vinyl_stat(kind=@vinyl.SymbolicLink))
        _ => None
      }
  }
  let contents = match file.contents {
    Empty => @vinyl.Null
    Text(text) => @vinyl.Buffer(@utf8.encode(text))
    Buffer(bytes) => @vinyl.Buffer(bytes)
    Bytes(stream) => @vinyl.Stream(stream)
    Directory => @vinyl.Null
    Symlink(_) => @vinyl.Null
  }
  @vinyl.vinyl_file(
    cwd=file.cwd,
    base=file.base,
    path=Some(file.path),
    contents~,
    stat~,
    symlink=file.symlink_target(),
    source_map=file.source_map(),
    metadata=file.metadata,
  )
}

///|
fn write_vinyl_bytes(
  path : String,
  contents : Bytes,
  options : DestOptions,
) -> Result[Bool, @core.MulpError] {
  let writer = open_dest_writer(
    @utf8.encode(path),
    options.append,
    options.overwrite,
  )
  if !dest_writer_is_ready(writer) {
    Err(@core.file_system_error("cannot write file: " + path))
  } else if dest_writer_is_skipped(writer) {
    ignore(dest_writer_close(writer))
    Ok(false)
  } else if !dest_writer_write(writer, contents) {
    ignore(dest_writer_close(writer))
    Err(@core.file_system_error("cannot write file: " + path))
  } else if dest_writer_close(writer) {
    Ok(true)
  } else {
    Err(@core.file_system_error("cannot write file: " + path))
  }
}

///|
fn write_vinyl_stream(
  path : String,
  stream : @stream.ByteStream,
  options : DestOptions,
) -> Result[Bool, @core.MulpError] {
  let writer = open_dest_writer(
    @utf8.encode(path),
    options.append,
    options.overwrite,
  )
  if !dest_writer_is_ready(writer) {
    Err(@core.file_system_error("cannot write file: " + path))
  } else if dest_writer_is_skipped(writer) {
    ignore(dest_writer_close(writer))
    Ok(false)
  } else {
    while true {
      match stream.read_next() {
        Ok(Some(chunk)) =>
          if !dest_writer_write(writer, chunk) {
            ignore(dest_writer_close(writer))
            return Err(@core.file_system_error("cannot write file: " + path))
          }
        Ok(None) =>
          if dest_writer_close(writer) {
            return Ok(true)
          } else {
            return Err(@core.file_system_error("cannot write file: " + path))
          }
        Err(err) => {
          ignore(dest_writer_close(writer))
          return Err(err)
        }
      }
    }
    Ok(true)
  }
}

///|
fn write_vinyl_file(
  file : @vinyl.VinylFile,
  options : DestOptions,
) -> Result[Bool, @core.MulpError] {
  if file.is_null() && !file.is_directory() && !file.is_symbolic() {
    Ok(false)
  } else if file.is_symbolic() && file.symlink() is None {
    Err(
      @core.task_failed("dest", "symbolic vinyl file is missing symlink target"),
    )
  } else if file.is_symbolic() &&
    !options.overwrite &&
    path_exists(@utf8.encode(file.path().unwrap_or(""))) {
    Ok(false)
  } else if !file.is_directory() && !file.is_symbolic() {
    match file.contents() {
      @vinyl.Buffer(bytes) =>
        write_vinyl_bytes(
          file.path().unwrap_or(""),
          encode_dest_bytes(bytes, options.encoding),
          options,
        )
      @vinyl.Stream(stream) =>
        write_vinyl_stream(file.path().unwrap_or(""), stream, options)
      @vinyl.Null => Ok(false)
    }
  } else {
    let stream_file = stream_file_from_vinyl(file)
    let stream = @stream.file_stream([stream_file]).pipe(
      @stream.file_dest(file.base()),
    )
    match stream.next() {
      Ok(Some(_)) => Ok(true)
      Ok(None) => Ok(false)
      Err(err) => Err(err)
    }
  }
}

///|
fn apply_written_metadata(
  file : @vinyl.VinylFile,
  wrote : Bool,
) -> Result[Unit, @core.MulpError] {
  if !wrote || file.is_symbolic() {
    Ok(())
  } else {
    match file.stat() {
      Some(stat) => {
        match stat.mode() {
          Some(mode) =>
            if !chmod_path(@utf8.encode(file.path().unwrap_or("")), mode) {
              return Err(
                @core.file_system_error(
                  "cannot chmod file: " + file.path().unwrap_or(""),
                ),
              )
            }
          None => ()
        }
        match stat.mtime_ms() {
          Some(mtime_ms) =>
            if !set_mtime_path(
                @utf8.encode(file.path().unwrap_or("")),
                mtime_ms,
              ) {
              return Err(
                @core.file_system_error(
                  "cannot update file time: " + file.path().unwrap_or(""),
                ),
              )
            }
          None => ()
        }
        Ok(())
      }
      None => Ok(())
    }
  }
}

///|
fn dest_output_files(
  file : @vinyl.VinylFile,
  options : DestOptions,
) -> Array[@vinyl.VinylFile] {
  let output = if options.inline_sourcemaps {
    inline_source_map_file(file)
  } else {
    file
  }
  let files = [output]
  if !options.inline_sourcemaps {
    match options.sourcemaps {
      Some(location) =>
        match source_map_file(output, location~) {
          Some(map) => files.push(map)
          None => ()
        }
      None => ()
    }
  }
  files
}

///|
fn path_parts(path : String) -> Array[String] {
  let parts : Array[String] = []
  for part in path.split("/") {
    let owned = part.to_owned()
    if owned != "" {
      parts.push(owned)
    }
  }
  parts
}

///|
fn relative_path_from(base : String, target : String) -> String {
  let base_parts = path_parts(base)
  let target_parts = path_parts(target)
  let mut index = 0
  while index < base_parts.length() &&
        index < target_parts.length() &&
        base_parts[index] == target_parts[index] {
    index = index + 1
  }
  let relative : Array[String] = []
  for _ in index..<base_parts.length() {
    relative.push("..")
  }
  for part_index in index..<target_parts.length() {
    relative.push(target_parts[part_index])
  }
  if relative.length() == 0 {
    "."
  } else {
    relative.join("/")
  }
}

///|
pub fn dest(out_dir : String) -> VinylTransform {
  dest_with_options(out_dir, dest_options())
}

///|
pub fn dest_with_options(
  out_dir : String,
  options : DestOptions,
) -> VinylTransform {
  fn(input : @vinyl.VinylFile) -> VinylFileStream {
    let base = resolve_dest_dir(out_dir, options.cwd)
    let prepared = with_dest_file(input, options, base)
    let state : DestState = {
      files: dest_output_files(prepared, options),
      index: 0,
    }
    vinyl_file_stream_from_pull(fn() -> Result[
      @vinyl.VinylFile?,
      @core.MulpError,
    ] {
      if state.index >= state.files.length() {
        Ok(None)
      } else {
        let file = state.files[state.index]
        match write_vinyl_file(file, options) {
          Ok(wrote) =>
            match apply_written_metadata(file, wrote) {
              Ok(_) => {
                state.index = state.index + 1
                Ok(Some(file))
              }
              Err(err) => Err(err)
            }
          Err(err) => Err(err)
        }
      }
    })
  }
}

///|
pub fn symlink(out_dir : String) -> VinylTransform {
  symlink_with_options(out_dir, dest_options())
}

///|
pub fn symlink_with_options(
  out_dir : String,
  options : DestOptions,
) -> VinylTransform {
  fn(input : @vinyl.VinylFile) -> VinylFileStream {
    let base = resolve_dest_dir(out_dir, options.cwd)
    let absolute_target = match input.symlink() {
      Some(path) => path
      None => input.path().unwrap_or("")
    }
    let target = if options.relative_symlinks {
      relative_path_from(base, absolute_target)
    } else {
      absolute_target
    }
    let prepared = @vinyl.vinyl_file(
      cwd=options.cwd,
      base~,
      path=Some(join_dest_path(base, input.relative())),
      contents=@vinyl.Null,
      stat=Some(@vinyl.vinyl_stat(kind=@vinyl.SymbolicLink)),
      symlink=Some(target),
      source_map=input.source_map(),
      metadata=input.metadata(),
    )
    let state : DestState = { files: [prepared], index: 0 }
    vinyl_file_stream_from_pull(fn() -> Result[
      @vinyl.VinylFile?,
      @core.MulpError,
    ] {
      if state.index >= state.files.length() {
        Ok(None)
      } else {
        let file = state.files[state.index]
        match write_vinyl_file(file, options) {
          Ok(wrote) =>
            match apply_written_metadata(file, wrote) {
              Ok(_) => {
                state.index = state.index + 1
                Ok(Some(file))
              }
              Err(err) => Err(err)
            }
          Err(err) => Err(err)
        }
      }
    })
  }
}

///|
priv type NativeDestWriter

///|
#borrow(path)
extern "c" fn open_dest_writer(
  path : Bytes,
  append : Bool,
  overwrite : Bool,
) -> NativeDestWriter = "mulp_vinyl_fs_open_dest_writer"

///|
#borrow(writer)
extern "c" fn dest_writer_is_ready(writer : NativeDestWriter) -> Bool = "mulp_vinyl_fs_dest_writer_is_ready"

///|
#borrow(writer)
extern "c" fn dest_writer_is_skipped(writer : NativeDestWriter) -> Bool = "mulp_vinyl_fs_dest_writer_is_skipped"

///|
#borrow(writer, chunk)
extern "c" fn dest_writer_write(
  writer : NativeDestWriter,
  chunk : Bytes,
) -> Bool = "mulp_vinyl_fs_dest_writer_write"

///|
#borrow(writer)
extern "c" fn dest_writer_close(writer : NativeDestWriter) -> Bool = "mulp_vinyl_fs_dest_writer_close"

///|
#borrow(path)
extern "c" fn chmod_path(path : Bytes, mode : Int) -> Bool = "mulp_vinyl_fs_chmod_path"

///|
#borrow(path)
extern "c" fn set_mtime_path(path : Bytes, mtime_ms : Int64) -> Bool = "mulp_vinyl_fs_set_mtime_path"

///|
#borrow(path)
extern "c" fn path_exists(path : Bytes) -> Bool = "mulp_vinyl_fs_path_exists"
