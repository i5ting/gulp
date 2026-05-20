///|
test "mulp command renders help" {
  inspect(
    mulp_command_output(["--help"]),
    content=(
      #|Usage: mulp [options] [task]
      #|
      #|Options:
      #|  --help                 Show this help.
      #|  -v, --version          Show the mulp version.
      #|  --cwd <dir>            Run as if started in dir.
      #|  --silent               Suppress all stdout output.
      #|  --quiet                Suppress non-error output.
      #|  --gulpfile <path>      Use an explicit gulp.mbtx.
      #|  --mulpfile <path>      Alias for --gulpfile.
      #|  --file <path>          Alias for --gulpfile.
      #|  -T, --tasks            List tasks.
      #|  --tasks-simple         Alias for --tasks.
      #|  --tasks-json           List tasks as JSON (for editor integrations).
      #|  --tree <task>          Render a task tree.
      #|  --watch <task>         Run a task in watch mode.
    ),
  )
}

///|
test "terminal output preserves existing task newlines" {
  let output = command_terminal_output({
    status: 0,
    stdout: "task output\n",
    stderr: "task warning\n",
  })
  inspect(output.stdout, content="task output\n")
  inspect(output.stderr, content="task warning\n")
}

///|
test "terminal output adds one newline for builtin text" {
  let output = command_terminal_output({
    status: 0,
    stdout: "0.1.0",
    stderr: "Config error: missing task",
  })
  inspect(output.stdout, content="0.1.0\n")
  inspect(output.stderr, content="Config error: missing task\n")
}

///|
test "real script discovery searches current directory then parents" {
  let found = discover_real_config(cwd="cmd/mulp/fixtures/mbtx-workspace/src").unwrap()
  inspect(found.path, content="cmd/mulp/fixtures/mbtx-workspace/gulp.mbtx")
}

///|
test "mulp command runs discovered mulp mbtx default task" {
  let result = mulp_command_result_from(
    cwd="cmd/mulp/fixtures/mbtx-workspace/src",
    args=[],
  )
  inspect(result.status, content="0")
  inspect(result.stderr, content="")
  inspect(result.stdout, content="mulp mbtx fixture: build\n")
}

///|
test "mulp command runs discovered mulp mbtx explicit task" {
  let result = mulp_command_result_from(
    cwd="cmd/mulp/fixtures/mbtx-workspace/src",
    args=["build"],
  )
  inspect(result.status, content="0")
  inspect(result.stderr, content="")
  inspect(result.stdout, content="mulp mbtx fixture: build\n")
}

///|
test "mulp command runs discovered mulp mbtx clean task" {
  let result = mulp_command_result_from(
    cwd="cmd/mulp/fixtures/mbtx-workspace/src",
    args=["clean"],
  )
  inspect(result.status, content="0")
  inspect(result.stderr, content="")
  inspect(result.stdout, content="mulp mbtx fixture: clean\n")
}

///|
test "mulp command proxies task listing to discovered mulp mbtx" {
  let result = mulp_command_result_from(
    cwd="cmd/mulp/fixtures/mbtx-workspace/src",
    args=["--tasks"],
  )
  inspect(result.status, content="0")
  inspect(result.stderr, content="")
  inspect(result.stdout, content="build\nclean\nwatch\n")
}

///|
test "mulp command accepts tasks-simple alias" {
  let result = mulp_command_result_from(
    cwd="cmd/mulp/fixtures/mbtx-workspace/src",
    args=["--tasks-simple"],
  )
  inspect(result.status, content="0")
  inspect(result.stderr, content="")
  inspect(result.stdout, content="build\nclean\nwatch\n")
}

///|
test "mulp command accepts short tasks alias" {
  let result = mulp_command_result_from(
    cwd="cmd/mulp/fixtures/mbtx-workspace/src",
    args=["-T"],
  )
  inspect(result.status, content="0")
  inspect(result.stderr, content="")
  inspect(result.stdout, content="build\nclean\nwatch\n")
}

///|
test "mulp command proxies task tree to discovered mulp mbtx" {
  let result = mulp_command_result_from(
    cwd="cmd/mulp/fixtures/mbtx-workspace/src",
    args=["--tree", "build"],
  )
  inspect(result.status, content="0")
  inspect(result.stderr, content="")
  inspect(result.stdout, content="root\n  build\n  clean\n  watch\n")
}

///|
test "mulp command proxies watch to discovered mulp mbtx" {
  let result = mulp_command_result_from(
    cwd="cmd/mulp/fixtures/mbtx-workspace/src",
    args=["--watch", "build"],
  )
  inspect(result.status, content="0")
  inspect(result.stderr, content="")
  inspect(result.stdout, content="mulp mbtx fixture: build\n")
}

///|
test "mulp command propagates unknown task failure from mulp mbtx" {
  let result = mulp_command_result_from(
    cwd="cmd/mulp/fixtures/mbtx-workspace/src",
    args=["missing"],
  )
  inspect(result.status, content="1")
  inspect(result.stdout, content="")
  inspect(result.stderr, content="Unknown task: missing\n")
}

///|
test "mulp command propagates watch task failure from mulp mbtx" {
  let result = mulp_command_result_from(
    cwd="cmd/mulp/fixtures/mbtx-workspace/src",
    args=["--watch", "missing"],
  )
  inspect(result.status, content="1")
  inspect(result.stdout, content="")
  inspect(result.stderr, content="Unknown task: missing\n")
}

///|
test "mulp command renders version without config" {
  let result = mulp_command_result_from(cwd="cmd/mulp/fixtures/missing", args=[
    "--version",
  ])
  inspect(result.status, content="0")
  inspect(result.stdout, content="0.1.0")
  inspect(result.stderr, content="")
}

///|
test "mulp command accepts short version alias without config" {
  let result = mulp_command_result_from(cwd="cmd/mulp/fixtures/missing", args=[
    "-v",
  ])
  inspect(result.status, content="0")
  inspect(result.stdout, content="0.1.0")
  inspect(result.stderr, content="")
}

///|
test "mulp command applies cwd before config discovery" {
  let result = mulp_command_result_from(cwd="cmd/mulp/fixtures/missing", args=[
    "--cwd", "../workspace/app/src", "build",
  ])
  inspect(result.status, content="0")
  inspect(result.stderr, content="")
  inspect(result.stdout, content="mulp fixture: build\n")
}

///|
test "real config discovery searches current directory then parents" {
  let found = discover_real_config(cwd="cmd/mulp/fixtures/workspace/app/src").unwrap()
  inspect(found.path, content="cmd/mulp/fixtures/workspace/app/gulp.mbtx")
}

///|
test "real config loading resolves explicit relative config path" {
  let found = load_real_config(
    cwd="cmd/mulp/fixtures/workspace/app",
    config_path=Some("../mulp.mbt"),
  ).unwrap()
  inspect(found.path, content="cmd/mulp/fixtures/workspace/mulp.mbt")
  inspect(
    @cli.parse_config(found.contents).unwrap().mulp.entry,
    content="root/build",
  )
}

///|
test "real config loading accepts mulpfile alias" {
  let result = mulp_command_result_from(cwd="cmd/mulp/fixtures/workspace/app", args=[
    "--mulpfile", "../mulp.mbt",
  ])
  inspect(result.status, content="255")
  inspect(result.stdout, content="")
}

///|
test "real config loading accepts gulpfile alias for mulp mbt" {
  let result = mulp_command_result_from(cwd="cmd/mulp/fixtures/workspace", args=[
    "--gulpfile", "app/build/mulp.mbt",
  ])
  inspect(result.status, content="0")
  inspect(result.stderr, content="")
  inspect(result.stdout, content="mulp fixture: app\n")
}

///|
test "mulp command infers entry from explicit mulpfile package path" {
  let result = mulp_command_result_from(cwd="cmd/mulp/fixtures/workspace", args=[
    "--mulpfile", "app/build/mulp.mbt",
  ])
  inspect(result.status, content="0")
  inspect(result.stderr, content="")
  inspect(result.stdout, content="mulp fixture: app\n")
}

///|
test "mulp command resolves default task from real config" {
  let result = mulp_command_result_from(
    cwd="cmd/mulp/fixtures/workspace/app/src",
    args=[],
  )
  inspect(result.status, content="0")
  inspect(result.stderr, content="")
  inspect(result.stdout, content="mulp fixture: app\n")
}

///|
test "mulp command resolves explicit task from real config" {
  let result = mulp_command_result_from(
    cwd="cmd/mulp/fixtures/workspace/app/src",
    args=["build"],
  )
  inspect(result.status, content="0")
  inspect(result.stderr, content="")
  inspect(result.stdout, content="mulp fixture: build\n")
}

///|
test "mulp example project runs src build entry" {
  let result = mulp_command_result_from(cwd="examples/basic", args=[])
  inspect(result.status, content="0")
  inspect(result.stderr, content="")
  inspect(result.stdout, content="example build\n")
}

///|
test "mulp example project runs parallel sleep tasks concurrently" {
  let result = mulp_command_result_from(cwd="examples/basic", args=["parallel"])
  inspect(result.status, content="0")
  inspect(result.stderr, content="")
  inspect(result.stdout, content="parallel ok\n")
}

///|
test "mulp example project accepts watch task command" {
  let result = mulp_command_result_from(cwd="examples/basic", args=[
    "--watch", "build",
  ])
  inspect(result.status, content="0")
  inspect(result.stderr, content="")
  inspect(result.stdout, content="example build\n")
}

///|
test "mulp example project works with only mulp mbt" {
  let build = mulp_command_result_from(cwd="examples/basic", args=["build"])
  inspect(build.status, content="0")
  inspect(build.stdout, content="example build\n")
  let tasks = mulp_command_result_from(cwd="examples/basic", args=["--tasks"])
  inspect(tasks.status, content="0")
  inspect(tasks.stdout, content="build\nclean\nwatch\nparallel\n")
  let tree = mulp_command_result_from(cwd="examples/basic", args=[
    "--tree", "build",
  ])
  inspect(tree.status, content="0")
  inspect(tree.stdout, content="root\n  build\n  clean\n  watch\n  parallel\n")
  let watch = mulp_command_result_from(cwd="examples/basic", args=[
    "--watch", "build",
  ])
  inspect(watch.status, content="0")
  inspect(watch.stdout, content="example build\n")
}

///|
test "mulp command lists configured default task" {
  let result = mulp_command_output_from(
    cwd="cmd/mulp/fixtures/workspace/app/src",
    args=["--tasks"],
  )
  inspect(result is Ok(_), content="true")
  match result {
    Ok(output) => inspect(output, content="app\nbuild\n")
    Err(_) => ()
  }
}

///|
test "mulp command renders requested task tree placeholder" {
  let result = mulp_command_output_from(
    cwd="cmd/mulp/fixtures/workspace/app/src",
    args=["--tree", "build"],
  )
  inspect(result is Ok(_), content="true")
  match result {
    Ok(output) => inspect(output, content="root\n  app\n  build\n")
    Err(_) => ()
  }
}

///|
test "mulp command reports errors with nonzero status" {
  let result = mulp_command_result_from(
    cwd="cmd/mulp/fixtures/workspace/app/src",
    args=["--unknown"],
  )
  inspect(result.status, content="1")
  inspect(result.stdout, content="")
  inspect(result.stderr, content="Config error: unknown flag: --unknown")
}

///|
test "mulp command reports missing config with nonzero status" {
  let result = mulp_command_result_from(cwd="cmd/mulp/fixtures/missing", args=[])
  inspect(result.status, content="1")
  inspect(
    result.stderr,
    content="Config error: could not find gulp.mbtx from cmd/mulp/fixtures/missing",
  )
}

///|
test "mulp command does not fall back to legacy toml config" {
  let result = mulp_command_result_from(cwd="cmd/mulp/fixtures/legacy-only", args=[])
  inspect(result.status, content="1")
  inspect(result.stdout, content="")
  inspect(
    result.stderr,
    content="Config error: could not find gulp.mbtx from cmd/mulp/fixtures/legacy-only",
  )
}

///|
test "mulp command reports CLI usage errors with nonzero status" {
  let result = mulp_command_result_from(
    cwd="cmd/mulp/fixtures/workspace/app/src",
    args=["--tree"],
  )
  inspect(result.status, content="1")
  inspect(result.stdout, content="")
  inspect(result.stderr, content="Config error: missing task for --tree")
}

///|
test "mulp command reports multiple CLI actions with nonzero status" {
  let result = mulp_command_result_from(
    cwd="cmd/mulp/fixtures/workspace/app/src",
    args=["build", "--tasks"],
  )
  inspect(result.status, content="1")
  inspect(result.stdout, content="")
  inspect(result.stderr, content="Config error: multiple CLI actions")
}

///|
test "mulp command reports invalid config with nonzero status" {
  let result = mulp_command_result_from(
    cwd="cmd/mulp/fixtures/workspace/app/src",
    args=["--config", "../../moon.mod.json"],
  )
  inspect(result.status, content="1")
  inspect(result.stdout, content="")
  inspect(result.stderr, content="Config error: malformed line: {")
}

///|
test "mulp command propagates entry process failure status" {
  let result = mulp_command_result_from(cwd="cmd/mulp/fixtures/workspace/app", args=[
    "--config", "../mulp.mbt",
  ])
  inspect(result.status, content="255")
  inspect(result.stdout, content="")
}
