///|
fn through2_test_context() -> @core.Context {
  @core.new_context(
    cwd="/tmp",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
}

///|
fn test_file(path : String, text : String) -> @stream.File {
  @stream.file(
    cwd="/workspace",
    base="/workspace/src",
    path~,
    contents=Text(text),
  )
}

///|
test "through2-master objectTransformPassthrough maps to through_files" {
  let files = @stream.file_stream([test_file("/workspace/src/a.txt", "a")])
    .pipe(
      through_files(fn(
        file : @stream.File,
      ) -> Result[@stream.File?, @core.MulpError] {
        Ok(Some(file.with_path("/workspace/src/out.txt")))
      }),
    )
    .collect(through2_test_context())
    .unwrap()
  inspect(files.length(), content="1")
  inspect(files[0].basename(), content="out.txt")
}

///|
test "through2-master asyncUndefinedSkipsEmission maps file None to filter" {
  let files = @stream.file_stream([
      test_file("/workspace/src/a.txt", "a"),
      test_file("/workspace/src/b.log", "b"),
    ])
    .pipe(
      through_files(fn(
        file : @stream.File,
      ) -> Result[@stream.File?, @core.MulpError] {
        if file.extname() == ".txt" {
          Ok(Some(file))
        } else {
          Ok(None)
        }
      }),
    )
    .collect(through2_test_context())
    .unwrap()
  inspect(files.length(), content="1")
  inspect(files[0].basename(), content="a.txt")
}

///|
test "through2-master asyncgenFanOut maps to through_files_flat" {
  let files = @stream.file_stream([test_file("/workspace/src/a.txt", "a")])
    .pipe(
      through_files_flat(fn(
        file : @stream.File,
      ) -> Result[Array[@stream.File], @core.MulpError] {
        Ok([file, file.with_path("/workspace/src/a.copy.txt")])
      }),
    )
    .collect(through2_test_context())
    .unwrap()
  inspect(files.length(), content="2")
  inspect(files[1].basename(), content="a.copy.txt")
}

///|
test "through2 file transform errors propagate" {
  let result = @stream.file_stream([test_file("/workspace/src/a.txt", "a")])
    .pipe(
      through_files(fn(
        _file : @stream.File,
      ) -> Result[@stream.File?, @core.MulpError] {
        Err(@core.stream_error("file-boom"))
      }),
    )
    .collect(through2_test_context())
  inspect(result is Err(_), content="true")
  inspect(result.unwrap_err().message(), content="Stream error: file-boom")
}

///|
test "through2 file_transformer creates independent stream transforms with flush" {
  let make = file_transformer(
    fn(file : @stream.File) -> Result[@stream.File?, @core.MulpError] {
      Ok(Some(file))
    },
    flush=Some(fn() -> Result[Array[@stream.File], @core.MulpError] {
      Ok([test_file("/workspace/src/end.txt", "end")])
    }),
  )
  let first = @stream.file_stream([test_file("/workspace/src/a.txt", "a")])
    .pipe_stream(make())
    .collect(through2_test_context())
    .unwrap()
  let second = @stream.file_stream([test_file("/workspace/src/b.txt", "b")])
    .pipe_stream(make())
    .collect(through2_test_context())
    .unwrap()
  inspect(first.length(), content="2")
  inspect(second.length(), content="2")
  inspect(first[1].basename(), content="end.txt")
  inspect(second[1].basename(), content="end.txt")
}

///|
test "through2 file_transformer close propagates to upstream" {
  let closed : Counter = { value: 0 }
  let mut emitted = false
  let source = @stream.file_stream_from_pull_with_close(
    fn() -> Result[@stream.File?, @core.MulpError] {
      if emitted {
        Ok(None)
      } else {
        emitted = true
        Ok(Some(test_file("/workspace/src/a.txt", "a")))
      }
    },
    fn() -> Unit { closed.value = closed.value + 1 },
  )
  let make = file_transformer(fn(
    file : @stream.File,
  ) -> Result[@stream.File?, @core.MulpError] {
    Ok(Some(file))
  })
  let output = source.pipe_stream(make())
  inspect(output.next().unwrap().unwrap().basename(), content="a.txt")
  output.close()
  inspect(closed.value, content="1")
}
