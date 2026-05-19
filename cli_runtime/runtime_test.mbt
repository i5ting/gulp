///|
test "cli runtime selects explicit mulpfile before discovered file" {
  let runtime = cli_runtime(
    cwd="/workspace/app",
    discovered=Some("/workspace/app/mulp.mbtx"),
    explicit=Some("/tmp/custom.mbtx"),
  )
  inspect(runtime.entry_file().unwrap(), content="/tmp/custom.mbtx")
}

///|
test "cli runtime falls back to discovered mulpfile" {
  let runtime = cli_runtime(
    cwd="/workspace/app",
    discovered=Some("/workspace/app/mulp.mbtx"),
  )
  inspect(runtime.entry_file().unwrap(), content="/workspace/app/mulp.mbtx")
}

///|
test "cli runtime renders help summary" {
  inspect(
    cli_help("0.1.0"),
    content="mulp 0.1.0\nUsage: mulp [--tasks|--tree|--watch] [task]",
  )
}

///|
test "cli runtime suggests completions for core flags" {
  inspect(
    cli_completions("--t"),
    content="[\"--tasks\", \"--tasks-simple\", \"--tree\"]",
  )
  inspect(
    cli_completions("--g"),
    content="[\"--gulpfile\"]",
  )
  inspect(
    cli_completions("-"),
    content="[\"-v\", \"-T\"]",
  )
}

///|
test "cli runtime checks compatible major versions" {
  inspect(select_compatible_version(["0.1.0", "0.2.0"], "^0.1").unwrap(), content="0.1.0")
  inspect(select_compatible_version(["0.1.0", "1.0.0"], "^1").unwrap(), content="1.0.0")
  inspect(select_compatible_version(["0.1.0"], "^2") is None, content="true")
}

///|
test "cli runtime renders compatibility hints" {
  inspect(
    compatibility_hint(
      requested="^1",
      available=["0.1.0", "0.2.0"],
    ),
    content="No local mulp version satisfies ^1; available: 0.1.0, 0.2.0",
  )
}
