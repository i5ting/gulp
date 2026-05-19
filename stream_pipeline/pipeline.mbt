///|
pub fn pipeline(
  source : @stream.FileStream,
  transforms : Array[@stream.Transform],
) -> @stream.FileStream {
  let mut current = source
  for transform in transforms {
    current = current.pipe(transform)
  }
  current
}

///|
pub fn to_through(transform : @stream.Transform) -> @stream.StreamTransform {
  fn(source : @stream.FileStream) -> @stream.FileStream {
    source.pipe(transform)
  }
}

///|
pub fn compose(
  source : @stream.FileStream,
  transforms : Array[@stream.StreamTransform],
) -> @stream.FileStream {
  let mut current = source
  for transform in transforms {
    current = current.pipe_stream(transform)
  }
  current
}

///|
pub fn lead(
  source : @stream.FileStream,
  ctx : @core.Context,
) -> Result[Unit, @core.MulpError] {
  match source.collect(ctx) {
    Ok(_) => Ok(())
    Err(err) => Err(err)
  }
}
