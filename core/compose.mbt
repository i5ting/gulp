///|
pub fn series(tasks : Array[Task]) -> Task {
  new_composed_task("series", tasks, fn(ctx : Context) -> Result[Unit, MulpError] {
    for item in tasks {
      if ctx.is_cancelled() {
        return Err(task_failed(item.name, "cancelled"))
      }
      match item.execute(ctx) {
        Ok(_) => ()
        Err(err) => return Err(err)
      }
    }
    Ok(())
  })
}

///|
pub fn parallel(tasks : Array[Task]) -> Task {
  new_composed_task("parallel", tasks, fn(ctx : Context) -> Result[Unit, MulpError] {
    let mut first_error : MulpError? = None
    for item in tasks {
      if ctx.is_cancelled() {
        break
      }
      match item.execute(ctx) {
        Ok(_) => ()
        Err(err) => {
          if first_error is None {
            first_error = Some(err)
          }
          ctx.cancellation.cancel()
        }
      }
    }
    match first_error {
      Some(err) => Err(err)
      None => Ok(())
    }
  })
}
