///|
pub fn transform_bytes(
  map? : ByteMap? = None,
  flush? : ByteFlush? = None,
) -> ByteTransform {
  let map = match map {
    Some(map) => map
    None => fn(chunk : Bytes) -> Result[Bytes?, @core.MulpError] {
      Ok(Some(chunk))
    }
  }
  match flush {
    Some(flush) => through_bytes_with_flush(map, flush)
    None => through_bytes(map)
  }
}

///|
pub fn transform_bytes_flat(
  flat_map : ByteFlatMap,
  flush? : ByteFlush? = None,
) -> ByteTransform {
  match flush {
    Some(flush) => through_bytes_flat_with_flush(flat_map, flush)
    None => through_bytes_flat(flat_map)
  }
}

///|
pub fn object_transform_files(
  map? : FileMap? = None,
  flush? : FileFlush? = None,
) -> @stream.StreamTransform {
  let map = match map {
    Some(map) => map
    None => fn(file : @stream.File) -> Result[@stream.File?, @core.MulpError] {
      Ok(Some(file))
    }
  }
  file_transformer(map, flush~)()
}

///|
pub fn object_transform_files_flat(
  flat_map : FileFlatMap,
  flush? : FileFlush? = None,
) -> @stream.StreamTransform {
  match flush {
    Some(flush) => through_file_stream_flat_with_flush(flat_map, flush)
    None => through_file_stream_flat_with_flush(flat_map, empty_file_flush)
  }
}

///|
pub fn object_transform_vinyl(
  map? : VinylMap? = None,
  flush? : VinylFlush? = None,
) -> @vinyl_fs.VinylStreamTransform {
  let map = match map {
    Some(map) => map
    None => fn(file : @vinyl.VinylFile) -> Result[@vinyl.VinylFile?, @core.MulpError] {
      Ok(Some(file))
    }
  }
  vinyl_transformer(map, flush~)()
}

///|
pub fn object_transform_vinyl_flat(
  flat_map : VinylFlatMap,
  flush? : VinylFlush? = None,
) -> @vinyl_fs.VinylStreamTransform {
  match flush {
    Some(flush) => through_vinyl_stream_flat_with_flush(flat_map, flush)
    None => through_vinyl_stream_flat_with_flush(flat_map, empty_vinyl_flush)
  }
}

///|
pub fn ctor_bytes(
  map : ByteMap,
  flush? : ByteFlush? = None,
) -> () -> ByteTransform {
  byte_transformer(map, flush~)
}

///|
pub fn ctor_files(
  map : FileMap,
  flush? : FileFlush? = None,
) -> () -> @stream.StreamTransform {
  file_transformer(map, flush~)
}

///|
pub fn ctor_vinyl(
  map : VinylMap,
  flush? : VinylFlush? = None,
) -> () -> @vinyl_fs.VinylStreamTransform {
  vinyl_transformer(map, flush~)
}
