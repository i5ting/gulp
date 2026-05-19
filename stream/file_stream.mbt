///|
pub type PullFile = () -> Result[File?, @core.MulpError]

///|
pub type CloseFileStream = () -> Unit

///|
pub type Transform = (File) -> FileStream

///|
pub type StreamTransform = (FileStream) -> FileStream

///|
struct StreamPlugin {
  name : String
  config : Array[(String, String)]
  transform : Transform
}

///|
struct FileStream {
  pull : PullFile
  close : CloseFileStream
  mut closed : Bool
}

///|
priv struct ArrayFileState {
  files : Array[File]
  mut index : Int
}

///|
priv struct PipeState {
  upstream : FileStream
  transform : Transform
  mut current : FileStream?
}

///|
priv struct MergeState {
  streams : Array[FileStream]
  mut index : Int
}

///|
priv struct ConcatState {
  upstream : FileStream
  out_path : String
  mut emitted : Bool
}

///|
pub fn file_stream_from_pull(pull : PullFile) -> FileStream {
  file_stream_from_pull_with_close(pull, fn() -> Unit { () })
}

///|
pub fn file_stream_from_pull_with_close(
  pull : PullFile,
  close : CloseFileStream,
) -> FileStream {
  { pull, close, closed: false }
}

///|
pub fn error_stream(err : @core.MulpError) -> FileStream {
  file_stream_from_pull(fn() -> Result[File?, @core.MulpError] { Err(err) })
}

///|
pub fn plugin(name : String, transform : Transform) -> StreamPlugin {
  { name, config: [], transform }
}

///|
pub fn plugin_with_config(
  name : String,
  config : Array[(String, String)],
  transform : Transform,
) -> StreamPlugin {
  { name, config, transform }
}

///|
pub fn StreamPlugin::name(self : StreamPlugin) -> String {
  self.name
}

///|
pub fn StreamPlugin::config_value(self : StreamPlugin, key : String) -> String? {
  for item in self.config {
    if item.0 == key {
      return Some(item.1)
    }
  }
  None
}

///|
fn package_tail(name : String) -> String {
  let mut start = 0
  for index in 0..<name.length() {
    if name[index:index + 1] == "/" {
      start = index + 1
    }
  }
  name[start:].to_owned()
}

///|
pub fn plugin_package_name_is_valid(name : String) -> Bool {
  package_tail(name).has_prefix("mulp-")
}

///|
pub fn plugin_export_name(name : String) -> String {
  let tail = package_tail(name)
  if tail.has_prefix("mulp-") {
    tail[5:].to_owned()
  } else {
    tail
  }
}

///|
pub fn file_stream(files : Array[File]) -> FileStream {
  let state = { files, index: 0 }
  file_stream_from_pull(fn() -> Result[File?, @core.MulpError] {
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
pub fn merge_sources(streams : Array[FileStream]) -> FileStream {
  let state : MergeState = { streams, index: 0 }
  file_stream_from_pull_with_close(
    fn() -> Result[File?, @core.MulpError] {
      while state.index < state.streams.length() {
        match state.streams[state.index].next() {
          Ok(Some(file)) => return Ok(Some(file))
          Ok(None) => state.index = state.index + 1
          Err(err) => return Err(err)
        }
      }
      Ok(None)
    },
    fn() -> Unit {
      for stream in state.streams {
        stream.close()
      }
    },
  )
}

///|
pub fn FileStream::next(self : FileStream) -> Result[File?, @core.MulpError] {
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
pub fn FileStream::close(self : FileStream) -> Unit {
  if !self.closed {
    self.closed = true
    (self.close)()
  }
}

///|
pub fn FileStream::pipe(self : FileStream, transform : Transform) -> FileStream {
  let state = { upstream: self, transform, current: None }
  file_stream_from_pull_with_close(
    fn() -> Result[File?, @core.MulpError] {
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
pub fn FileStream::pipe_plugin(
  self : FileStream,
  plugin : StreamPlugin,
) -> FileStream {
  self.pipe(fn(file : File) -> FileStream {
    let inner = (plugin.transform)(file)
    file_stream_from_pull_with_close(
      fn() -> Result[File?, @core.MulpError] {
        match inner.next() {
          Ok(file) => Ok(file)
          Err(err) =>
            Err(
              @core.stream_error(
                "plugin " + plugin.name + ": " + err.message(),
              ),
            )
        }
      },
      fn() -> Unit { inner.close() },
    )
  })
}

///|
pub fn FileStream::pipe_stream(
  self : FileStream,
  transform : StreamTransform,
) -> FileStream {
  transform(self)
}

///|
pub fn filter_files(keep : (File) -> Bool) -> Transform {
  fn(file : File) -> FileStream {
    if keep(file) {
      file_stream([file])
    } else {
      file_stream([])
    }
  }
}

///|
pub fn map_files(map : (File) -> File) -> Transform {
  fn(file : File) -> FileStream { file_stream([map(file)]) }
}

///|
pub fn flat_map_files(map : (File) -> Array[File]) -> Transform {
  fn(file : File) -> FileStream { file_stream(map(file)) }
}

///|
pub fn rename_plugin(rename : (File) -> String) -> StreamPlugin {
  plugin("rename", fn(file : File) -> FileStream {
    file_stream([file.with_path(rename(file))])
  })
}

///|
pub fn filter_plugin(keep : (File) -> Bool) -> StreamPlugin {
  plugin("filter", filter_files(keep))
}

///|
fn concat_output_path(base : String, out_path : String) -> String {
  if out_path.has_prefix("/") {
    out_path
  } else if base.has_suffix("/") {
    base + out_path
  } else {
    base + "/" + out_path
  }
}

///|
fn concat_buffer(contents : FileContents) -> Result[String, @core.MulpError] {
  match contents {
    Empty => Ok("")
    Text(text) => Ok(text)
    Buffer(bytes) => Ok(@utf8.decode_lossy(bytes))
    Bytes(_) | Directory | Symlink(_) =>
      Err(@core.stream_error("concat only supports buffered files"))
  }
}

///|
pub fn concat_plugin(out_path : String) -> StreamTransform {
  fn(upstream : FileStream) -> FileStream {
    let state : ConcatState = { upstream, out_path, emitted: false }
    file_stream_from_pull_with_close(
      fn() -> Result[File?, @core.MulpError] {
        if state.emitted {
          return Ok(None)
        }
        state.emitted = true
        let mut first : File? = None
        let mut contents = ""
        while true {
          match state.upstream.next() {
            Ok(Some(file)) => {
              if first is None {
                first = Some(file)
              }
              match concat_buffer(file.contents) {
                Ok(text) => contents = contents + text
                Err(err) => return Err(err)
              }
            }
            Ok(None) =>
              match first {
                Some(file) =>
                  return Ok(
                    Some({
                      cwd: file.cwd,
                      base: file.base,
                      path: concat_output_path(file.base, state.out_path),
                      contents: Text(contents),
                      metadata: [],
                      source_map: None,
                    }),
                  )
                None => return Ok(None)
              }
            Err(err) => return Err(err)
          }
        }
        Ok(None)
      },
      fn() -> Unit { state.upstream.close() },
    )
  }
}

///|
pub fn test_plugin(
  plugin : StreamPlugin,
  files : Array[File],
) -> Result[Array[File], @core.MulpError] {
  let ctx = @core.new_context(
    cwd=".",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  file_stream(files).pipe_plugin(plugin).collect(ctx)
}

///|
pub fn FileStream::collect(
  self : FileStream,
  ctx : @core.Context,
) -> Result[Array[File], @core.MulpError] {
  let files : Array[File] = []
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
