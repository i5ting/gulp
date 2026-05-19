///|
pub type VinylMap = (@vinyl.VinylFile) -> Result[
  @vinyl.VinylFile?,
  @core.MulpError,
]

///|
pub type VinylFlatMap = (@vinyl.VinylFile) -> Result[
  Array[@vinyl.VinylFile],
  @core.MulpError,
]

///|
pub type VinylFlush = () -> Result[Array[@vinyl.VinylFile], @core.MulpError]

///|
priv struct VinylThroughState {
  upstream : @vinyl_fs.VinylFileStream
  flat_map : VinylFlatMap
  flush : VinylFlush
  mut pending : Array[@vinyl.VinylFile]
  mut pending_index : Int
  mut upstream_ended : Bool
  mut flushed : Bool
}

///|
fn empty_vinyl_flush() -> Result[Array[@vinyl.VinylFile], @core.MulpError] {
  Ok([])
}

///|
fn VinylThroughState::next_pending(
  self : VinylThroughState,
) -> @vinyl.VinylFile? {
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
fn vinyl_state(
  upstream : @vinyl_fs.VinylFileStream,
  flat_map : VinylFlatMap,
  flush : VinylFlush,
) -> VinylThroughState {
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
pub fn through_vinyl_stream_flat_with_flush(
  flat_map : VinylFlatMap,
  flush : VinylFlush,
) -> @vinyl_fs.VinylStreamTransform {
  fn(upstream : @vinyl_fs.VinylFileStream) -> @vinyl_fs.VinylFileStream {
    let state = vinyl_state(upstream, flat_map, flush)
    @vinyl_fs.vinyl_file_stream_from_pull_with_close(
      fn() -> Result[@vinyl.VinylFile?, @core.MulpError] {
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
pub fn through_vinyl_flat(flat_map : VinylFlatMap) -> @vinyl_fs.VinylTransform {
  fn(file : @vinyl.VinylFile) -> @vinyl_fs.VinylFileStream {
    match flat_map(file) {
      Ok(files) => @vinyl_fs.vinyl_file_stream(files)
      Err(err) => @vinyl_fs.vinyl_error_stream(err)
    }
  }
}

///|
pub fn through_vinyl(map : VinylMap) -> @vinyl_fs.VinylTransform {
  through_vinyl_flat(fn(
    file : @vinyl.VinylFile,
  ) -> Result[Array[@vinyl.VinylFile], @core.MulpError] {
    match map(file) {
      Ok(Some(output)) => Ok([output])
      Ok(None) => Ok([])
      Err(err) => Err(err)
    }
  })
}

///|
pub fn vinyl_transformer(
  map : VinylMap,
  flush? : VinylFlush? = None,
) -> () -> @vinyl_fs.VinylStreamTransform {
  fn() -> @vinyl_fs.VinylStreamTransform {
    let flat_map = fn(
      file : @vinyl.VinylFile,
    ) -> Result[Array[@vinyl.VinylFile], @core.MulpError] {
      match map(file) {
        Ok(Some(output)) => Ok([output])
        Ok(None) => Ok([])
        Err(err) => Err(err)
      }
    }
    match flush {
      Some(flush) => through_vinyl_stream_flat_with_flush(flat_map, flush)
      None => through_vinyl_stream_flat_with_flush(flat_map, empty_vinyl_flush)
    }
  }
}
