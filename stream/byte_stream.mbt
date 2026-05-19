///|
pub type PullChunk = () -> Result[Bytes?, @core.MulpError]

///|
pub type CloseStream = () -> Unit

///|
struct ByteStream {
  pull : PullChunk
  close : CloseStream
  mut consumed : Bool
}

///|
pub fn byte_stream_from_pull(pull : PullChunk) -> ByteStream {
  byte_stream_from_pull_with_close(pull, fn() -> Unit { () })
}

///|
pub fn byte_stream_from_pull_with_close(
  pull : PullChunk,
  close : CloseStream,
) -> ByteStream {
  { pull, close, consumed: false }
}

///|
pub fn byte_stream_from_bytes(chunks : Array[Bytes]) -> ByteStream {
  let mut index = 0
  byte_stream_from_pull(fn() -> Result[Bytes?, @core.MulpError] {
    if index >= chunks.length() {
      Ok(None)
    } else {
      let chunk = chunks[index]
      index = index + 1
      Ok(Some(chunk))
    }
  })
}

///|
pub fn byte_stream(chunks : Array[String]) -> ByteStream {
  let mut index = 0
  byte_stream_from_pull(fn() -> Result[Bytes?, @core.MulpError] {
    if index >= chunks.length() {
      Ok(None)
    } else {
      let chunk = @utf8.encode(chunks[index])
      index = index + 1
      Ok(Some(chunk))
    }
  })
}

///|
pub fn ByteStream::read_next(
  self : ByteStream,
) -> Result[Bytes?, @core.MulpError] {
  if self.consumed {
    Err(@core.stream_error("byte stream already consumed"))
  } else {
    match (self.pull)() {
      Ok(Some(chunk)) => Ok(Some(chunk))
      Ok(None) => {
        self.close()
        Ok(None)
      }
      Err(err) => {
        self.close()
        Err(err)
      }
    }
  }
}

///|
pub fn ByteStream::close(self : ByteStream) -> Unit {
  if !self.consumed {
    self.consumed = true
    (self.close)()
  }
}

///|
pub fn ByteStream::read_all(
  self : ByteStream,
  max_bytes~ : Int,
) -> Result[String, @core.MulpError] {
  let chunks : Array[String] = []
  let mut total = 0
  while true {
    match self.read_next() {
      Ok(Some(chunk)) => {
        total = total + chunk.length()
        if total > max_bytes {
          self.close()
          return Err(
            @core.stream_error("byte stream exceeded \{max_bytes} bytes"),
          )
        }
        chunks.push(@utf8.decode_lossy(chunk))
      }
      Ok(None) => return Ok(chunks.join(""))
      Err(err) => return Err(err)
    }
  }
  Ok(chunks.join(""))
}

///|
pub fn ByteStream::read_all_bytes(
  self : ByteStream,
  max_bytes~ : Int,
) -> Result[Bytes, @core.MulpError] {
  let chunks : Array[Bytes] = []
  let mut total = 0
  while true {
    match self.read_next() {
      Ok(Some(chunk)) => {
        total = total + chunk.length()
        if total > max_bytes {
          self.close()
          return Err(
            @core.stream_error("byte stream exceeded \{max_bytes} bytes"),
          )
        }
        if chunk.length() > 0 {
          chunks.push(chunk)
        }
      }
      Ok(None) => return Ok(concat_byte_chunks(chunks, total))
      Err(err) => return Err(err)
    }
  }
  Ok(concat_byte_chunks(chunks, total))
}

///|
fn concat_byte_chunks(chunks : Array[Bytes], total : Int) -> Bytes {
  if total == 0 {
    Bytes::make(0, b'\x00')
  } else {
    let mut chunk_index = 0
    let mut chunk_offset = 0
    Bytes::makei(total, fn(_index : Int) -> Byte {
      let chunk = chunks[chunk_index]
      let byte = chunk[chunk_offset]
      chunk_offset = chunk_offset + 1
      if chunk_offset >= chunk.length() {
        chunk_index = chunk_index + 1
        chunk_offset = 0
      }
      byte
    })
  }
}
