///|
pub type ByteTransform = (@stream.ByteStream) -> @stream.ByteStream

///|
pub type ByteMap = (Bytes) -> Result[Bytes?, @core.MulpError]

///|
pub type ByteFlatMap = (Bytes) -> Result[Array[Bytes], @core.MulpError]

///|
pub type ByteFlush = () -> Result[Array[Bytes], @core.MulpError]

///|
priv struct ByteThroughState {
  upstream : @stream.ByteStream
  flat_map : ByteFlatMap
  flush : ByteFlush
  mut pending : Array[Bytes]
  mut pending_index : Int
  mut upstream_ended : Bool
  mut flushed : Bool
}

///|
fn empty_byte_flush() -> Result[Array[Bytes], @core.MulpError] {
  Ok([])
}

///|
fn ByteThroughState::next_pending(self : ByteThroughState) -> Bytes? {
  if self.pending_index < self.pending.length() {
    let chunk = self.pending[self.pending_index]
    self.pending_index = self.pending_index + 1
    Some(chunk)
  } else {
    self.pending = []
    self.pending_index = 0
    None
  }
}

///|
fn through_bytes_state(
  upstream : @stream.ByteStream,
  flat_map : ByteFlatMap,
  flush : ByteFlush,
) -> ByteThroughState {
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
pub fn through_bytes_flat_with_flush(
  flat_map : ByteFlatMap,
  flush : ByteFlush,
) -> ByteTransform {
  fn(upstream : @stream.ByteStream) -> @stream.ByteStream {
    let state = through_bytes_state(upstream, flat_map, flush)
    @stream.byte_stream_from_pull_with_close(
      fn() -> Result[Bytes?, @core.MulpError] {
        while true {
          match state.next_pending() {
            Some(chunk) => return Ok(Some(chunk))
            None => ()
          }
          if state.upstream_ended {
            if state.flushed {
              return Ok(None)
            } else {
              match (state.flush)() {
                Ok(chunks) => {
                  state.pending = chunks
                  state.pending_index = 0
                  state.flushed = true
                }
                Err(err) => return Err(err)
              }
            }
          } else {
            match state.upstream.read_next() {
              Ok(Some(chunk)) =>
                match (state.flat_map)(chunk) {
                  Ok(chunks) => {
                    state.pending = chunks
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
pub fn through_bytes_flat(flat_map : ByteFlatMap) -> ByteTransform {
  through_bytes_flat_with_flush(flat_map, empty_byte_flush)
}

///|
pub fn through_bytes_with_flush(
  map : ByteMap,
  flush : ByteFlush,
) -> ByteTransform {
  through_bytes_flat_with_flush(
    fn(chunk : Bytes) -> Result[Array[Bytes], @core.MulpError] {
      match map(chunk) {
        Ok(Some(output)) => Ok([output])
        Ok(None) => Ok([])
        Err(err) => Err(err)
      }
    },
    flush,
  )
}

///|
pub fn through_bytes(map : ByteMap) -> ByteTransform {
  through_bytes_with_flush(map, empty_byte_flush)
}

///|
pub fn byte_passthrough() -> ByteTransform {
  through_bytes(fn(chunk : Bytes) -> Result[Bytes?, @core.MulpError] {
    Ok(Some(chunk))
  })
}

///|
pub fn byte_transformer(
  map : ByteMap,
  flush? : ByteFlush? = None,
) -> () -> ByteTransform {
  fn() -> ByteTransform {
    match flush {
      Some(flush) => through_bytes_with_flush(map, flush)
      None => through_bytes(map)
    }
  }
}
