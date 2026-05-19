///|
test "watch_task_name extracts task from --watch args" {
  let result = watch_task_name(["--watch", "build"])
  inspect(result is Ok(_), content="true")
  match result {
    Ok(name) => inspect(name, content="build")
    Err(_) => ()
  }
}

///|
test "watch_task_name errors when no task given" {
  let result = watch_task_name(["--watch"])
  inspect(result is Err(_), content="true")
}

///|
test "watch_task_name errors when args are empty" {
  let result = watch_task_name([])
  inspect(result is Err(_), content="true")
}
