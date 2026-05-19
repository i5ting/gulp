///|
pub struct MoonRunCommand {
  executable : String
  args : Array[String]
}

///|
pub fn is_mulp_script_path(path : String) -> Bool {
  path.has_suffix(".mbtx")
}

///|
fn trim_trailing_slash(value : String) -> String {
  if value != "/" && value.has_suffix("/") {
    value[:value.length() - 1].to_owned()
  } else {
    value
  }
}

///|
pub fn path_relative_to_root(path : String, moon_root~ : String) -> String {
  let root = trim_trailing_slash(moon_root)
  if path == root {
    "."
  } else if path.has_prefix(root + "/") {
    path[root.length() + 1:].to_owned()
  } else {
    path
  }
}

///|
pub fn infer_entry_from_mulp_path(path : String, moon_root~ : String) -> String {
  let suffix = "/mulp.mbt"
  let dir = if path.has_suffix(suffix) {
    path[:path.length() - suffix.length()].to_owned()
  } else {
    path
  }
  let root = trim_trailing_slash(moon_root)
  if dir == root {
    "."
  } else if dir.has_prefix(root + "/") {
    dir[root.length() + 1:].to_owned()
  } else {
    dir
  }
}

///|
fn configured_or_inferred_entry(
  config : MulpConfig,
  inferred_entry : String?,
) -> Result[String, @core.MulpError] {
  if config.mulp.entry != "" {
    Ok(config.mulp.entry)
  } else {
    match inferred_entry {
      Some(entry) =>
        if entry == "" {
          Err(@core.config_error("missing mulp.entry"))
        } else {
          Ok(entry)
        }
      None => Err(@core.config_error("missing mulp.entry"))
    }
  }
}

///|
pub fn moon_run_script_command(
  script_path : String,
  script_args : Array[String],
) -> MoonRunCommand {
  let args = ["run", "--quiet", "--target", "native", script_path, "--"]
  for arg in script_args {
    args.push(arg)
  }
  { executable: "moon", args }
}

///|
pub fn moon_run_command_with_entry(
  config : MulpConfig,
  cli : CliArgs,
  inferred_entry~ : String?,
) -> Result[MoonRunCommand, @core.MulpError] {
  let entry = match configured_or_inferred_entry(config, inferred_entry) {
    Ok(value) => value
    Err(err) => return Err(err)
  }
  match cli.action {
    Run(task) => Ok({ executable: "moon", args: ["run", entry, "--", task] })
    Watch(task) =>
      Ok({ executable: "moon", args: ["run", entry, "--", "--watch", task] })
    ListTasks =>
      Ok({ executable: "moon", args: ["run", entry, "--", "--tasks"] })
    TasksJson =>
      Ok({ executable: "moon", args: ["run", entry, "--", "--tasks-json"] })
    Tree(task) =>
      Ok({ executable: "moon", args: ["run", entry, "--", "--tree", task] })
  }
}

///|
pub fn moon_run_command(
  config : MulpConfig,
  cli : CliArgs,
) -> Result[MoonRunCommand, @core.MulpError] {
  moon_run_command_with_entry(config, cli, inferred_entry=None)
}
