///|
pub type FileMap = (@stream.File) -> Result[@stream.File?, @core.MulpError]

///|
pub type FileFlatMap = (@stream.File) -> Result[
  Array[@stream.File],
  @core.MulpError,
]

///|
pub type FileFlush = () -> Result[Array[@stream.File], @core.MulpError]

///|
priv struct FileThroughState {
  upstream : @stream.FileStream
  flat_map : FileFlatMap
  flush : FileFlush
  mut pending : Array[@stream.File]
  mut pending_index : Int
  mut upstream_ended : Bool
  mut flushed : Bool
}

///|
fn empty_file_flush() -> Result[Array[@stream.File], @core.MulpError] {
  Ok([])
}

///|
fn FileThroughState::next_pending(self : FileThroughState) -> @stream.File? {
  if self.pending_index < self.pending.length() {
    let file = self.pending[self.pending_index]
    self.pending_index = self.pending_index + 1
    Some(file)
  } else {
    self.pending = []
    self.pending_index = 0
    None
  }
}

///|
fn file_state(
  upstream : @stream.FileStream,
  flat_map : FileFlatMap,
  flush : FileFlush,
) -> FileThroughState {
  {
    upstream,
    flat_map,
    flush,
    pending: [],
    pending_index: 0,
    upstream_ended: false,
    flushed: false,
  }
}

///|
pub fn through_file_stream_flat_with_flush(
  flat_map : FileFlatMap,
  flush : FileFlush,
) -> @stream.StreamTransform {
  fn(upstream : @stream.FileStream) -> @stream.FileStream {
    let state = file_state(upstream, flat_map, flush)
    @stream.file_stream_from_pull_with_close(
      fn() -> Result[@stream.File?, @core.MulpError] {
        while true {
          match state.next_pending() {
            Some(file) => return Ok(Some(file))
            None => ()
          }
          if state.upstream_ended {
            if state.flushed {
              return Ok(None)
            } else {
              match (state.flush)() {
                Ok(files) => {
                  state.pending = files
                  state.pending_index = 0
                  state.flushed = true
                }
                Err(err) => return Err(err)
              }
            }
          } else {
            match state.upstream.next() {
              Ok(Some(file)) =>
                match (state.flat_map)(file) {
                  Ok(files) => {
                    state.pending = files
                    state.pending_index = 0
                  }
                  Err(err) => {
                    state.upstream.close()
                    return Err(err)
                  }
                }
              Ok(None) => state.upstream_ended = true
              Err(err) => return Err(err)
            }
          }
        }
        Ok(None)
      },
      fn() -> Unit {
        state.pending = []
        state.upstream.close()
      },
    )
  }
}

///|
pub fn through_files_flat(flat_map : FileFlatMap) -> @stream.Transform {
  fn(file : @stream.File) -> @stream.FileStream {
    match flat_map(file) {
      Ok(files) => @stream.file_stream(files)
      Err(err) => @stream.error_stream(err)
    }
  }
}

///|
pub fn through_files(map : FileMap) -> @stream.Transform {
  through_files_flat(fn(
    file : @stream.File,
  ) -> Result[Array[@stream.File], @core.MulpError] {
    match map(file) {
      Ok(Some(output)) => Ok([output])
      Ok(None) => Ok([])
      Err(err) => Err(err)
    }
  })
}

///|
pub fn file_transformer(
  map : FileMap,
  flush? : FileFlush? = None,
) -> () -> @stream.StreamTransform {
  fn() -> @stream.StreamTransform {
    match flush {
      Some(flush) =>
        through_file_stream_flat_with_flush(
          fn(
            file : @stream.File,
          ) -> Result[Array[@stream.File], @core.MulpError] {
            match map(file) {
              Ok(Some(output)) => Ok([output])
              Ok(None) => Ok([])
              Err(err) => Err(err)
            }
          },
          flush,
        )
      None =>
        through_file_stream_flat_with_flush(
          fn(
            file : @stream.File,
          ) -> Result[Array[@stream.File], @core.MulpError] {
            match map(file) {
              Ok(Some(output)) => Ok([output])
              Ok(None) => Ok([])
              Err(err) => Err(err)
            }
          },
          empty_file_flush,
        )
    }
  }
}
