///|
fn worker_pool_file(name : String) -> @stream.File {
  @stream.file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/" + name,
    contents=@stream.Text(name),
  )
}

///|
async test "async file worker pool limits concurrency" {
  let files = [
    worker_pool_file("a.txt"),
    worker_pool_file("b.txt"),
    worker_pool_file("c.txt"),
    worker_pool_file("d.txt"),
  ]
  let pool = async_file_worker_pool(concurrency=2)
  let token = @core.new_cancellation_token()
  let ctx = @core.new_context(cwd="/workspace", now_ms=9100L, cancellation=token)
  let mut running = 0
  let mut max_running = 0
  let handled : Array[String] = []
  let result = pool.drain(@stream.file_stream(files), ctx, async fn(
    file : @stream.File,
    _ctx : @core.Context,
  ) -> Result[Unit, @core.MulpError] {
    running = running + 1
    if running > max_running {
      max_running = running
    }
    @async.sleep(20)
    handled.push(file.relative_path())
    running = running - 1
    Ok(())
  })
  inspect(result is Ok(_), content="true")
  inspect(max_running, content="2")
  inspect(handled.length(), content="4")
}

///|
async test "async file worker pool cancels token and aggregates worker errors" {
  let pool = async_file_worker_pool(concurrency=2)
  let token = @core.new_cancellation_token()
  let ctx = @core.new_context(cwd="/workspace", now_ms=9200L, cancellation=token)
  let result = pool.drain(
    @stream.file_stream([
      worker_pool_file("a.txt"),
      worker_pool_file("b.txt"),
    ]),
    ctx,
    async fn(file : @stream.File, _ctx : @core.Context) -> Result[Unit, @core.MulpError] {
      if file.relative_path() == "a.txt" {
        Err(@core.stream_error("failed " + file.relative_path()))
      } else {
        @async.sleep(20)
        Ok(())
      }
    },
  )
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Parallel failed: a.txt: Stream error: failed a.txt",
  )
  inspect(token.is_cancelled(), content="true")
}

///|
async test "async file worker pool stops pulling after cancellation" {
  let files = [
    worker_pool_file("a.txt"),
    worker_pool_file("b.txt"),
  ]
  let mut index = 0
  let pulls : Array[String] = []
  let stream = @stream.file_stream_from_pull(fn() -> Result[@stream.File?, @core.MulpError] {
    pulls.push("pull:\{index}")
    if index >= files.length() {
      Ok(None)
    } else {
      let file = files[index]
      index = index + 1
      Ok(Some(file))
    }
  })
  let pool = async_file_worker_pool(concurrency=1)
  let token = @core.new_cancellation_token()
  let ctx = @core.new_context(cwd="/workspace", now_ms=9250L, cancellation=token)
  let result = pool.drain(stream, ctx, async fn(
    file : @stream.File,
    _ctx : @core.Context,
  ) -> Result[Unit, @core.MulpError] {
    @async.sleep(1)
    if file.relative_path() == "a.txt" {
      Err(@core.stream_error("failed " + file.relative_path()))
    } else {
      Ok(())
    }
  })
  inspect(result is Err(_), content="true")
  inspect(pulls, content="[\"pull:0\"]")
}

///|
async test "async file worker pool rejects non-positive concurrency" {
  let pool = async_file_worker_pool(concurrency=0)
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=9300L,
    cancellation=@core.new_cancellation_token(),
  )
  let result = pool.drain(@stream.file_stream([]), ctx, fn(
    _file : @stream.File,
    _ctx : @core.Context,
  ) -> Result[Unit, @core.MulpError] {
    Ok(())
  })
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Stream error: worker pool concurrency must be positive",
  )
}
