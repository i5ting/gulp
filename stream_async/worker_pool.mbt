///|
pub type AsyncFileWorker = async (
  @stream.File,
  @core.Context
) -> Result[Unit, @core.MulpError]

///|
struct AsyncFileWorkerPool {
  concurrency : Int
}

///|
pub fn async_file_worker_pool(concurrency~ : Int) -> AsyncFileWorkerPool {
  { concurrency, }
}

///|
pub async fn AsyncFileWorkerPool::drain(
  self : AsyncFileWorkerPool,
  stream : @stream.FileStream,
  ctx : @core.Context,
  worker : AsyncFileWorker,
) -> Result[Unit, @core.MulpError] {
  if self.concurrency <= 0 {
    return Err(@core.stream_error("worker pool concurrency must be positive"))
  }
  let semaphore = @async.Semaphore(self.concurrency)
  let errors : Array[(String, String)] = []
  @async.with_task_group(group => {
    let mut done = false
    while !done && !ctx.is_cancelled() {
      semaphore.acquire()
      if ctx.is_cancelled() {
        semaphore.release()
        stream.close()
        done = true
      } else {
        match stream.next() {
          Ok(Some(file)) => {
            group.spawn_bg(allow_failure=true, async fn() -> Unit {
              match worker(file, ctx) {
                Ok(_) => ()
                Err(err) => {
                  errors.push((file.relative_path(), err.message()))
                  ctx.cancellation.cancel()
                }
              }
              semaphore.release()
            })
          }
          Ok(None) => {
            semaphore.release()
            done = true
          }
          Err(err) => {
            semaphore.release()
            errors.push(("stream", err.message()))
            ctx.cancellation.cancel()
            stream.close()
            done = true
          }
        }
      }
    }
  })
  if ctx.is_cancelled() {
    stream.close()
  }
  if errors.length() > 0 {
    Err(@core.parallel_failed(errors))
  } else if ctx.is_cancelled() {
    Err(@core.task_failed("worker pool", "cancelled"))
  } else {
    Ok(())
  }
}
