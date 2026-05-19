///|
test "task not found error renders task name" {
  let err = task_not_found("build")
  inspect(err.message(), content="Task not found: build")
}

///|
test "task failed error renders cause" {
  let err = task_failed("build", "exit code 1")
  inspect(err.message(), content="Task failed: build: exit code 1")
}

///|
test "config error renders detail" {
  let err = config_error("missing mulp.default")
  inspect(err.message(), content="Config error: missing mulp.default")
}
