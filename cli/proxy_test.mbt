///|
fn sample_config() -> MulpConfig {
  let source =
    #|[package]
    #|name = "site-build"
    #|version = "0.1.0"
    #|
    #|[mulp]
    #|entry = "src/build"
    #|default = "build"
  parse_config(source).unwrap()
}

///|
test "moon run command uses default task" {
  let cli = parse_args([], default_task=sample_config().mulp.default_task).unwrap()
  let command = moon_run_command(sample_config(), cli).unwrap()
  inspect(command.executable, content="moon")
  inspect(command.args.join(" "), content="run src/build -- build")
}

///|
test "moon run script command proxies empty args to mbtx file" {
  let command = moon_run_script_command("/workspace/app/gulp.mbtx", [])
  inspect(command.executable, content="moon")
  inspect(
    command.args.join(" "),
    content="run --quiet --target native /workspace/app/gulp.mbtx --",
  )
}

///|
test "moon run script command proxies task args to mbtx file" {
  let command = moon_run_script_command("/workspace/app/gulp.mbtx", ["build"])
  inspect(
    command.args.join(" "),
    content="run --quiet --target native /workspace/app/gulp.mbtx -- build",
  )
}

///|
test "moon run script command proxies watch args to mbtx file" {
  let command = moon_run_script_command("/workspace/app/gulp.mbtx", [
    "--watch", "build",
  ])
  inspect(
    command.args.join(" "),
    content="run --quiet --target native /workspace/app/gulp.mbtx -- --watch build",
  )
}

///|
test "moon run command uses explicit task" {
  let cli = parse_args(["test"], default_task=sample_config().mulp.default_task).unwrap()
  let command = moon_run_command(sample_config(), cli).unwrap()
  inspect(command.args.join(" "), content="run src/build -- test")
}

///|
test "moon run command proxies watch task arguments" {
  let cli = parse_args(
    ["--watch", "build"],
    default_task=sample_config().mulp.default_task,
  ).unwrap()
  let command = moon_run_command(sample_config(), cli).unwrap()
  inspect(command.args.join(" "), content="run src/build -- --watch build")
}

///|
test "moon run command can use inferred entry" {
  let source =
    #|[package]
    #|name = "site-build"
    #|
    #|[mulp]
    #|default = "build"
  let config = parse_config(source).unwrap()
  let cli = parse_args([], default_task=config.mulp.default_task).unwrap()
  let command = moon_run_command_with_entry(
    config,
    cli,
    inferred_entry=Some("app/build"),
  ).unwrap()
  inspect(command.args.join(" "), content="run app/build -- build")
}

///|
test "infer entry from mulp file path relative to moon root" {
  inspect(
    infer_entry_from_mulp_path(
      "/workspace/app/build/mulp.mbt",
      moon_root="/workspace",
    ),
    content="app/build",
  )
}

///|
test "moon run command proxies list tasks action" {
  let cli = parse_args(
    ["--tasks"],
    default_task=sample_config().mulp.default_task,
  ).unwrap()
  let command = moon_run_command(sample_config(), cli).unwrap()
  inspect(command.args.join(" "), content="run src/build -- --tasks")
}

///|
test "moon run command proxies tasks json action" {
  let cli = parse_args(
    ["--tasks-json"],
    default_task=sample_config().mulp.default_task,
  ).unwrap()
  let command = moon_run_command(sample_config(), cli).unwrap()
  inspect(command.args.join(" "), content="run src/build -- --tasks-json")
}

///|
test "moon run command proxies tree action" {
  let cli = parse_args(
    ["--tree", "build"],
    default_task=sample_config().mulp.default_task,
  ).unwrap()
  let command = moon_run_command(sample_config(), cli).unwrap()
  inspect(command.args.join(" "), content="run src/build -- --tree build")
}
