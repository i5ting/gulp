///|
fn ok_task(_ctx : @core.Context) -> Result[Unit, @core.MulpError] {
  Ok(())
}

///|
fn sample_registry() -> @core.Registry {
  let registry = @core.new_registry()
  ignore(@core.task(registry, "build", ok_task))
  ignore(@core.task(registry, "test", ok_task))
  registry
}

///|
test "render cli output lists registered task names" {
  let cli = parse_args(["--tasks"], default_task="build").unwrap()
  inspect(render_cli_output(cli, sample_registry()).unwrap(), content="build\ntest")
}

///|
test "render cli output renders task tree" {
  let cli = parse_args(["--tree", "build"], default_task="build").unwrap()
  inspect(
    render_cli_output(cli, sample_registry()).unwrap(),
    content="root\n  build\n  test",
  )
}

///|
test "render cli output rejects run actions" {
  let cli = parse_args(["build"], default_task="default").unwrap()
  let result = render_cli_output(cli, sample_registry())
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Config error: run is not a renderable CLI output",
  )
}

///|
test "render cli output tasks-json produces JSON object" {
  let cli = parse_args(["--tasks-json"], default_task="build").unwrap()
  let json = render_cli_output(cli, sample_registry()).unwrap()
  inspect(json.has_prefix("{\"label\":\"Tasks\",\"nodes\":[{\"label\":\"build\""), content="true")
}
