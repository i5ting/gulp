///|
fn facade_file(path : String, text : String) -> @stream.File {
  @stream.file(
    cwd="/workspace",
    base="/workspace/src",
    path~,
    contents=Text(text),
  )
}

///|
fn facade_vinyl(path : String, text : String) -> @vinyl.VinylFile {
  @vinyl.vinyl_file(
    cwd="/workspace",
    base="/workspace/src",
    path=Some(path),
    contents=@vinyl.Buffer(@utf8.encode(text)),
  )
}

///|
test "through2 facade transform_bytes defaults to passthrough" {
  let output = transform_bytes()(@stream.byte_stream(["he", "llo"]))
  inspect(output.read_all(max_bytes=8).unwrap(), content="hello")
}

///|
test "through2 facade transform_bytes supports map and flush" {
  let output = transform_bytes(
    map=Some(fn(_chunk : Bytes) -> Result[Bytes?, @core.MulpError] {
      Ok(Some(@utf8.encode("X")))
    }),
    flush=Some(fn() -> Result[Array[Bytes], @core.MulpError] {
      Ok([@utf8.encode("END")])
    }),
  )(@stream.byte_stream(["a", "b"]))
  inspect(output.read_all(max_bytes=16).unwrap(), content="XXEND")
}

///|
test "through2 facade object_transform_files defaults to stream passthrough" {
  let output = @stream.file_stream([facade_file("/workspace/src/app.txt", "a")])
    .pipe_stream(object_transform_files())
    .collect(through2_test_context())
    .unwrap()
  inspect(output.length(), content="1")
  inspect(output[0].basename(), content="app.txt")
}

///|
test "through2 facade object_transform_files supports map and flush" {
  let output = @stream.file_stream([facade_file("/workspace/src/app.txt", "a")])
    .pipe_stream(
      object_transform_files(
        map=Some(fn(file : @stream.File) -> Result[@stream.File?, @core.MulpError] {
          Ok(Some(file.with_path("/workspace/src/app.out.txt")))
        }),
        flush=Some(fn() -> Result[Array[@stream.File], @core.MulpError] {
          Ok([facade_file("/workspace/src/end.txt", "end")])
        }),
      ),
    )
    .collect(through2_test_context())
    .unwrap()
  inspect(output.length(), content="2")
  inspect(output[0].basename(), content="app.out.txt")
  inspect(output[1].basename(), content="end.txt")
}

///|
test "through2 facade object_transform_vinyl supports map and flush" {
  let output = @vinyl_fs.vinyl_file_stream([
      facade_vinyl("/workspace/src/app.txt", "a"),
    ])
    .pipe_stream(
      object_transform_vinyl(
        map=Some(fn(file : @vinyl.VinylFile) -> Result[@vinyl.VinylFile?, @core.MulpError] {
          Ok(Some(file.with_path("/workspace/src/app.out.txt")))
        }),
        flush=Some(fn() -> Result[Array[@vinyl.VinylFile], @core.MulpError] {
          Ok([facade_vinyl("/workspace/src/end.txt", "end")])
        }),
      ),
    )
    .collect(vinyl_test_context())
    .unwrap()
  inspect(output.length(), content="2")
  inspect(output[0].basename(), content="app.out.txt")
  inspect(output[1].basename(), content="end.txt")
}

///|
test "through2 facade ctor aliases create reusable factories" {
  let make = ctor_files(fn(file : @stream.File) -> Result[@stream.File?, @core.MulpError] {
    Ok(Some(file))
  })
  let first = @stream.file_stream([facade_file("/workspace/src/a.txt", "a")])
    .pipe_stream(make())
    .collect(through2_test_context())
    .unwrap()
  let second = @stream.file_stream([facade_file("/workspace/src/b.txt", "b")])
    .pipe_stream(make())
    .collect(through2_test_context())
    .unwrap()
  inspect(first[0].basename(), content="a.txt")
  inspect(second[0].basename(), content="b.txt")
}
