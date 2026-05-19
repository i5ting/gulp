///|
async test "stream async task drains stream and records last run" {
  let started_ms = @async.now()
  let pulled : Array[String] = []
  let names = ["a.txt", "b.txt"]
  let mut index = 0
  let stream = @stream.file_stream_from_pull(fn() -> Result[
    @stream.File?,
    @core.MulpError,
  ] {
    if index >= names.length() {
      Ok(None)
    } else {
      let name = names[index]
      pulled.push(name)
      index = index + 1
      Ok(
        Some(
          @stream.file(
            cwd="/workspace",
            base="/workspace/src",
            path="/workspace/src/" + name,
            contents=@stream.Text(name),
          ),
        ),
      )
    }
  })
  let task = async_task_from_stream("files", stream)
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=8000L,
    cancellation=@core.new_cancellation_token(),
  )
  let result = task.execute(ctx)
  inspect(result is Ok(_), content="true")
  inspect(@async.now() >= started_ms, content="true")
  inspect(pulled, content="[\"a.txt\", \"b.txt\"]")
  inspect(@core.last_run_async(task) is Some(8000L), content="true")
}

///|
async test "stream async task returns stream errors" {
  let started_ms = @async.now()
  let task = async_task_from_stream(
    "files",
    @stream.error_stream(@core.stream_error("read failed")),
  )
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=8100L,
    cancellation=@core.new_cancellation_token(),
  )
  let result = task.execute(ctx)
  inspect(result is Err(_), content="true")
  inspect(@async.now() >= started_ms, content="true")
  inspect(result.unwrap_err().message(), content="Stream error: read failed")
  inspect(@core.last_run_async(task) is None, content="true")
}

///|
async test "stream async task rejects conflicting completion signals" {
  let task = async_task_from_stream("files", @stream.file_stream([]), signals=[
    @core.CallbackCompletion,
  ])
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=8150L,
    cancellation=@core.new_cancellation_token(),
  )
  let result = task.execute(ctx)
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Task failed: files: multiple async completion signals: stream, callback",
  )
  inspect(@core.last_run_async(task) is None, content="true")
}

///|
async test "stream async task rejects process completion conflict" {
  let task = async_task_from_stream("files", @stream.file_stream([]), signals=[
    @core.ProcessCompletion,
  ])
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=8160L,
    cancellation=@core.new_cancellation_token(),
  )
  let result = task.execute(ctx)
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Task failed: files: multiple async completion signals: stream, process",
  )
  inspect(@core.last_run_async(task) is None, content="true")
}
