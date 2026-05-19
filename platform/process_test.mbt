///|
test "process runner executes moon run command" {
  let command = process_command(
    "moon",
    ["run", ".", "--", "build's task"],
  ).with_cwd("./platform/fixtures/echo")
  let result = run_process(command).unwrap()
  inspect(result.exit_code, content="0")
  inspect(result.stdout, content="mulp fixture: build's task\n")
  inspect(result.stderr, content="")
}

///|
test "process runner separates stdout stderr and renders logs" {
  let command = process_command(
    "sh",
    ["-c", "printf 'out'; printf 'err' >&2; exit 7"],
  )
  let result = run_process(command).unwrap()
  inspect(result.exit_code, content="7")
  inspect(result.stdout, content="out")
  inspect(result.stderr, content="err")
  inspect(result.render_log(), content="[stdout]\nout\n[stderr]\nerr\n")
}
