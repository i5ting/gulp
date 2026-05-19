///|
pub(all) enum CliDispatch {
  Proxy(MoonRunCommand)
  Rendered(String)
}

///|
fn config_path_from_args(
  args : Array[String],
) -> Result[String?, @core.MulpError] {
  let mut index = 0
  while index < args.length() {
    if args[index] == "--config" ||
      args[index] == "--mulpfile" ||
      args[index] == "--file" ||
      args[index] == "--gulpfile" {
      if index + 1 >= args.length() {
        return Err(@core.config_error("missing value for " + args[index]))
      }
      return Ok(Some(args[index + 1]))
    }
    index = index + 1
  }
  Ok(None)
}

///|
fn args_without_config_path(
  args : Array[String],
) -> Result[Array[String], @core.MulpError] {
  let filtered : Array[String] = []
  let mut index = 0
  while index < args.length() {
    if args[index] == "--config" ||
      args[index] == "--mulpfile" ||
      args[index] == "--file" ||
      args[index] == "--gulpfile" {
      if index + 1 >= args.length() {
        return Err(@core.config_error("missing value for " + args[index]))
      }
      index = index + 2
    } else {
      filtered.push(args[index])
      index = index + 1
    }
  }
  Ok(filtered)
}

///|
fn normalize_script_args(args : Array[String]) -> Array[String] {
  let normalized : Array[String] = []
  for arg in args {
    if arg == "--tasks-simple" || arg == "-T" {
      normalized.push("--tasks")
    } else {
      normalized.push(arg)
    }
  }
  normalized
}

///|
pub fn dispatch_cli(
  cwd~ : String,
  files~ : Array[(String, String)],
  args~ : Array[String],
  registry~ : @core.Registry,
) -> Result[CliDispatch, @core.MulpError] {
  let config_path = match config_path_from_args(args) {
    Ok(path) => path
    Err(err) => return Err(err)
  }
  let document = match load_config(cwd~, config_path~, files) {
    Ok(found) => found
    Err(err) => return Err(err)
  }
  if is_mulp_script_path(document.path) {
    match parse_args(args, default_task="") {
      Ok(_) => ()
      Err(err) => return Err(err)
    }
    let script_args = match args_without_config_path(args) {
      Ok(filtered) => filtered
      Err(err) => return Err(err)
    }
    return Ok(Proxy(moon_run_script_command(document.path, normalize_script_args(script_args))))
  }
  let config = match parse_config(document.contents) {
    Ok(parsed) => parsed
    Err(err) => return Err(err)
  }
  let cli = match parse_args(args, default_task=config.mulp.default_task) {
    Ok(parsed) => parsed
    Err(err) => return Err(err)
  }
  match cli.action {
    Run(_) | Watch(_) =>
      match
        moon_run_command_with_entry(
          config,
          cli,
          inferred_entry=Some(
            infer_entry_from_mulp_path(document.path, moon_root=cwd),
          ),
        ) {
        Ok(command) => Ok(Proxy(command))
        Err(err) => Err(err)
      }
    ListTasks | TasksJson | Tree(_) =>
      match render_cli_output(cli, registry) {
        Ok(output) => Ok(Rendered(output))
        Err(err) => Err(err)
      }
  }
}
