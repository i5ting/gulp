///|
test "parse args runs default task when no task is provided" {
  let cli = parse_args([], default_task="build").unwrap()
  inspect(cli.config_path is None, content="true")
  match cli.action {
    Run(task) => inspect(task, content="build")
    _ => inspect("wrong action", content="run")
  }
}

///|
test "parse args runs explicit task" {
  let cli = parse_args(["test"], default_task="build").unwrap()
  match cli.action {
    Run(task) => inspect(task, content="test")
    _ => inspect("wrong action", content="run")
  }
}

///|
test "parse args lists tasks" {
  let cli = parse_args(["--tasks"], default_task="build").unwrap()
  inspect(cli.config_path is None, content="true")
  inspect(cli.action is ListTasks, content="true")
}

///|
test "parse args accepts gulp tasks-simple alias" {
  let cli = parse_args(["--tasks-simple"], default_task="build").unwrap()
  inspect(cli.action is ListTasks, content="true")
}

///|
test "parse args accepts gulp short tasks alias" {
  let cli = parse_args(["-T"], default_task="build").unwrap()
  inspect(cli.action is ListTasks, content="true")
}

///|
test "parse args renders tree for explicit task" {
  let cli = parse_args(["--tree", "build"], default_task="default").unwrap()
  match cli.action {
    Tree(task) => inspect(task, content="build")
    _ => inspect("wrong action", content="tree")
  }
}

///|
test "parse args watches explicit task" {
  let cli = parse_args(["--watch", "build"], default_task="default").unwrap()
  match cli.action {
    Watch(task) => inspect(task, content="build")
    _ => inspect("wrong action", content="watch")
  }
}

///|
test "parse args records config path and explicit task" {
  let cli = parse_args(
    ["--config", "path/to/mulp.mbt", "build"],
    default_task="default",
  ).unwrap()
  inspect(cli.config_path.unwrap(), content="path/to/mulp.mbt")
  match cli.action {
    Run(task) => inspect(task, content="build")
    _ => inspect("wrong action", content="run")
  }
}

///|
test "parse args accepts mulpfile aliases" {
  let mulpfile = parse_args(
    ["--mulpfile", "build/mulp.mbt", "build"],
    default_task="default",
  ).unwrap()
  inspect(mulpfile.config_path.unwrap(), content="build/mulp.mbt")
  let file = parse_args(
    ["--file", "build/mulp.mbt", "--tasks"],
    default_task="default",
  ).unwrap()
  inspect(file.config_path.unwrap(), content="build/mulp.mbt")
  inspect(file.action is ListTasks, content="true")
  let gulpfile = parse_args(
    ["--gulpfile", "build/mulp.mbt", "build"],
    default_task="default",
  ).unwrap()
  inspect(gulpfile.config_path.unwrap(), content="build/mulp.mbt")
}

///|
test "parse args rejects unknown flags" {
  let result = parse_args(["--wat"], default_task="build")
  inspect(result is Err(_), content="true")
  inspect(result.unwrap_err().message(), content="Config error: unknown flag: --wat")
}

///|
test "parse args tasks json action" {
  let cli = parse_args(["--tasks-json"], default_task="build").unwrap()
  inspect(cli.action is TasksJson, content="true")
}

///|
test "parse args silent sets log_level 0" {
  let cli = parse_args(["--silent", "build"], default_task="default").unwrap()
  inspect(cli.log_level, content="0")
  match cli.action {
    Run(t) => inspect(t, content="build")
    _      => inspect("wrong action", content="run")
  }
}

///|
test "parse args quiet sets log_level 1" {
  let cli = parse_args(["--quiet"], default_task="build").unwrap()
  inspect(cli.log_level, content="1")
  inspect(cli.action is Run(_), content="true")
}

///|
test "parse args default log_level is 2" {
  let cli = parse_args(["build"], default_task="default").unwrap()
  inspect(cli.log_level, content="2")
}

///|
test "parse args silent can precede config flag" {
  let cli = parse_args(
    ["--silent", "--config", "build/mulp.mbt", "build"],
    default_task="default",
  ).unwrap()
  inspect(cli.log_level, content="0")
  inspect(cli.config_path.unwrap(), content="build/mulp.mbt")
}
