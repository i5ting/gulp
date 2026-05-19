///|
fn vinyl_test_context() -> @core.Context {
  @core.new_context(
    cwd="/tmp",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
}

///|
fn test_vinyl(path : String, text : String) -> @vinyl.VinylFile {
  @vinyl.vinyl_file(
    cwd="/workspace",
    base="/workspace/src",
    path=Some(path),
    contents=@vinyl.Buffer(@utf8.encode(text)),
  )
}

///|
test "through2 objectTransform equivalent maps to through_vinyl" {
  let files = @vinyl_fs.vinyl_file_stream([
      test_vinyl("/workspace/src/a.txt", "a"),
    ])
    .pipe(
      through_vinyl(fn(
        file : @vinyl.VinylFile,
      ) -> Result[@vinyl.VinylFile?, @core.MulpError] {
        Ok(Some(file.with_path("/workspace/src/out.txt")))
      }),
    )
    .collect(vinyl_test_context())
    .unwrap()
  inspect(files.length(), content="1")
  inspect(files[0].basename(), content="out.txt")
}

///|
test "through2 filter equivalent maps None to dropped vinyl files" {
  let files = @vinyl_fs.vinyl_file_stream([
      test_vinyl("/workspace/src/a.txt", "a"),
      test_vinyl("/workspace/src/b.log", "b"),
    ])
    .pipe(
      through_vinyl(fn(
        file : @vinyl.VinylFile,
      ) -> Result[@vinyl.VinylFile?, @core.MulpError] {
        if file.extname() == ".txt" {
          Ok(Some(file))
        } else {
          Ok(None)
        }
      }),
    )
    .collect(vinyl_test_context())
    .unwrap()
  inspect(files.length(), content="1")
  inspect(files[0].basename(), content="a.txt")
}

///|
test "through2 asyncgenFanOut equivalent maps to through_vinyl_flat" {
  let files = @vinyl_fs.vinyl_file_stream([
      test_vinyl("/workspace/src/a.txt", "a"),
    ])
    .pipe(
      through_vinyl_flat(fn(
        file : @vinyl.VinylFile,
      ) -> Result[Array[@vinyl.VinylFile], @core.MulpError] {
        Ok([file, file.with_path("/workspace/src/a.copy.txt")])
      }),
    )
    .collect(vinyl_test_context())
    .unwrap()
  inspect(files.length(), content="2")
  inspect(files[1].basename(), content="a.copy.txt")
}

///|
test "through2 vinyl_transformer creates independent stream transforms with flush" {
  let make = vinyl_transformer(
    fn(file : @vinyl.VinylFile) -> Result[@vinyl.VinylFile?, @core.MulpError] {
      Ok(Some(file))
    },
    flush=Some(fn() -> Result[Array[@vinyl.VinylFile], @core.MulpError] {
      Ok([test_vinyl("/workspace/src/end.txt", "end")])
    }),
  )
  let first = @vinyl_fs.vinyl_file_stream([
      test_vinyl("/workspace/src/a.txt", "a"),
    ])
    .pipe_stream(make())
    .collect(vinyl_test_context())
    .unwrap()
  let second = @vinyl_fs.vinyl_file_stream([
      test_vinyl("/workspace/src/b.txt", "b"),
    ])
    .pipe_stream(make())
    .collect(vinyl_test_context())
    .unwrap()
  inspect(first.length(), content="2")
  inspect(second.length(), content="2")
  inspect(first[1].basename(), content="end.txt")
  inspect(second[1].basename(), content="end.txt")
}

///|
test "through2 vinyl_transformer close propagates to upstream" {
  let closed : Counter = { value: 0 }
  let mut emitted = false
  let source = @vinyl_fs.vinyl_file_stream_from_pull_with_close(
    fn() -> Result[@vinyl.VinylFile?, @core.MulpError] {
      if emitted {
        Ok(None)
      } else {
        emitted = true
        Ok(Some(test_vinyl("/workspace/src/a.txt", "a")))
      }
    },
    fn() -> Unit { closed.value = closed.value + 1 },
  )
  let make = vinyl_transformer(fn(
    file : @vinyl.VinylFile,
  ) -> Result[@vinyl.VinylFile?, @core.MulpError] {
    Ok(Some(file))
  })
  let output = source.pipe_stream(make())
  inspect(output.next().unwrap().unwrap().basename(), content="a.txt")
  output.close()
  inspect(closed.value, content="1")
}
