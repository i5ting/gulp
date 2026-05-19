///|
pub(all) enum CliAction {
  Run(String)
  ListTasks
  TasksJson
  Tree(String)
  Watch(String)
}

///|
pub struct CliArgs {
  config_path : String?
  action      : CliAction
  log_level   : Int
}

///|
fn is_config_path_flag(arg : String) -> Bool {
  arg == "--config" || arg == "--mulpfile" || arg == "--file" || arg == "--gulpfile"
}

///|
pub fn parse_args(
  args : Array[String],
  default_task~ : String,
) -> Result[CliArgs, @core.MulpError] {
  let mut config_path : String? = None
  let mut action : CliAction = Run(default_task)
  let mut log_level : Int = 2
  let mut saw_action = false
  let mut index = 0
  while index < args.length() {
    let arg = args[index]
    if is_config_path_flag(arg) {
      if index + 1 >= args.length() {
        return Err(@core.config_error("missing value for " + arg))
      }
      config_path = Some(args[index + 1])
      index = index + 2
    } else if arg == "--tasks" || arg == "--tasks-simple" || arg == "-T" {
      if saw_action {
        return Err(@core.config_error("multiple CLI actions"))
      }
      action = ListTasks
      saw_action = true
      index = index + 1
    } else if arg == "--tasks-json" {
      if saw_action {
        return Err(@core.config_error("multiple CLI actions"))
      }
      action = TasksJson
      saw_action = true
      index = index + 1
    } else if arg == "--tree" {
      if saw_action {
        return Err(@core.config_error("multiple CLI actions"))
      }
      if index + 1 >= args.length() {
        return Err(@core.config_error("missing task for --tree"))
      }
      action = Tree(args[index + 1])
      saw_action = true
      index = index + 2
    } else if arg == "--watch" {
      if saw_action {
        return Err(@core.config_error("multiple CLI actions"))
      }
      if index + 1 >= args.length() {
        return Err(@core.config_error("missing task for --watch"))
      }
      action = Watch(args[index + 1])
      saw_action = true
      index = index + 2
    } else if arg == "--silent" {
      log_level = 0
      index = index + 1
    } else if arg == "--quiet" {
      log_level = 1
      index = index + 1
    } else if arg.has_prefix("--") {
      return Err(@core.config_error("unknown flag: " + arg))
    } else {
      if saw_action {
        return Err(@core.config_error("multiple CLI actions"))
      }
      action = Run(arg)
      saw_action = true
      index = index + 1
    }
  }
  Ok({ config_path, action, log_level })
}
