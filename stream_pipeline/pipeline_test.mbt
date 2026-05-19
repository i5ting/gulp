///|
fn pipeline_test_context() -> @core.Context {
  @core.new_context(
    cwd="/tmp",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
}

///|
test "stream pipeline composes transforms in order" {
  let input = @stream.file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/app.txt",
    contents=Text("hello"),
  )
  let rename = @stream.map_files(fn(file : @stream.File) -> @stream.File {
    file.with_path("/workspace/src/renamed.txt")
  })
  let output = pipeline(@stream.file_stream([input]), [rename])
    .collect(pipeline_test_context())
    .unwrap()
  inspect(output.length(), content="1")
  inspect(output[0].basename(), content="renamed.txt")
}

///|
test "stream pipeline accepts through2 file transforms" {
  let input = @stream.file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/app.txt",
    contents=Text("hello"),
  )
  let output = pipeline(@stream.file_stream([input]), [
      @through2.through_files(fn(
        file : @stream.File,
      ) -> Result[@stream.File?, @core.MulpError] {
        Ok(Some(file.with_path("/workspace/src/app.out.txt")))
      }),
    ])
    .collect(pipeline_test_context())
    .unwrap()
  inspect(output.length(), content="1")
  inspect(output[0].basename(), content="app.out.txt")
}

///|
test "stream pipeline composes stream transforms and to-through adapters" {
  let input = @stream.file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/app.txt",
    contents=Text("hello"),
  )
  let rename = to_through(
    @stream.map_files(fn(file : @stream.File) -> @stream.File {
      file.with_path("/workspace/src/renamed.txt")
    }),
  )
  let duplicate : @stream.StreamTransform = fn(
    source : @stream.FileStream,
  ) -> @stream.FileStream {
    source.pipe(fn(file : @stream.File) -> @stream.FileStream {
      @stream.file_stream([file, file.with_path(file.path + ".copy")])
    })
  }
  let output = compose(@stream.file_stream([input]), [rename, duplicate])
    .collect(pipeline_test_context())
    .unwrap()
  inspect(output.length(), content="2")
  inspect(output[0].basename(), content="renamed.txt")
  inspect(output[1].basename(), content="renamed.txt.copy")
}

///|
test "stream pipeline composes through2 file transformer with flush" {
  let input = @stream.file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/app.txt",
    contents=Text("hello"),
  )
  let make = @through2.file_transformer(
    fn(file : @stream.File) -> Result[@stream.File?, @core.MulpError] {
      Ok(Some(file))
    },
    flush=Some(fn() -> Result[Array[@stream.File], @core.MulpError] {
      Ok([
        @stream.file(
          cwd="/workspace",
          base="/workspace/src",
          path="/workspace/src/end.txt",
          contents=Text("end"),
        ),
      ])
    }),
  )
  let output = compose(@stream.file_stream([input]), [make()])
    .collect(pipeline_test_context())
    .unwrap()
  inspect(output.length(), content="2")
  inspect(output[1].basename(), content="end.txt")
}

///|
test "stream pipeline close closes source and active transform" {
  let events : Array[String] = []
  let mut emitted = false
  let source = @stream.file_stream_from_pull_with_close(
    fn() -> Result[@stream.File?, @core.MulpError] {
      if emitted {
        Ok(None)
      } else {
        emitted = true
        Ok(
          Some(
            @stream.file(
              cwd="/workspace",
              base="/workspace/src",
              path="/workspace/src/app.txt",
              contents=Text("hello"),
            ),
          ),
        )
      }
    },
    fn() -> Unit { events.push("source closed") },
  )
  let hold : @stream.StreamTransform = fn(
    upstream : @stream.FileStream,
  ) -> @stream.FileStream {
    upstream.pipe(fn(file : @stream.File) -> @stream.FileStream {
      @stream.file_stream_from_pull_with_close(
        fn() -> Result[@stream.File?, @core.MulpError] { Ok(Some(file)) },
        fn() -> Unit { events.push("transform closed") },
      )
    })
  }
  let output = compose(source, [hold])
  inspect(output.next().unwrap().unwrap().basename(), content="app.txt")
  output.close()
  inspect(events, content="[\"transform closed\", \"source closed\"]")
}

///|
test "stream pipeline propagates transform errors downstream" {
  let input = @stream.file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/app.txt",
    contents=Text("hello"),
  )
  let fail = to_through(fn(_file : @stream.File) -> @stream.FileStream {
    @stream.error_stream(@core.stream_error("pipeline failed"))
  })
  let result = compose(@stream.file_stream([input]), [fail]).collect(
    pipeline_test_context(),
  )
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Stream error: pipeline failed",
  )
}

///|
test "stream pipeline lead drains streams and returns errors" {
  let result = lead(
    @stream.error_stream(@core.stream_error("boom")),
    pipeline_test_context(),
  )
  inspect(result is Err(_), content="true")
  inspect(result.unwrap_err().message(), content="Stream error: boom")
}
