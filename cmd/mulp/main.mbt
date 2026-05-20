///|
pub fn mulp_command_output(_args : Array[String]) -> String {
  (
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
  )
}

///|
fn mulp_version() -> String {
  "0.1.0"
}

///|
pub struct CommandResult {
  status : Int
  stdout : String
  stderr : String
}

///|
priv struct CommandTerminalOutput {
  stdout : String
  stderr : String
}

///|
fn ensure_trailing_newline(value : String) -> String {
  if value == "" || value.has_suffix("\n") {
    value
  } else {
    value + "\n"
  }
}

///|
fn command_terminal_output(result : CommandResult) -> CommandTerminalOutput {
  {
    stdout: ensure_trailing_newline(result.stdout),
    stderr: ensure_trailing_newline(result.stderr),
  }
}

///|
fn strip_moon_dependency_noise(stdout : String) -> String {
  let kept : Array[String] = []
  for line in stdout.split("\n") {
    if line.has_prefix("Using cached ") {
      ()
    } else {
      kept.push(line.to_owned())
    }
  }
  kept.join("\n")
}

///|
fn execute_moon_run_command(
  command : @cli.MoonRunCommand,
  cwd : String,
) -> CommandResult {
  let run_cwd = match discover_moon_root(cwd~) {
    Ok(root) => root
    Err(_) => cwd
  }
  let process = @platform.process_command(command.executable, command.args).with_cwd(
    run_cwd,
  )
  match @platform.run_process(process) {
    Ok(result) =>
      {
        status: result.exit_code,
        stdout: strip_moon_dependency_noise(result.stdout),
        stderr: result.stderr,
      }
    Err(err) => { status: 1, stdout: "", stderr: err.message() }
  }
}

///|
fn append_command_result(
  target : CommandResult,
  source : CommandResult,
) -> CommandResult {
  {
    status: if target.status != 0 { target.status } else { source.status },
    stdout: target.stdout + source.stdout,
    stderr: target.stderr + source.stderr,
  }
}

///|
pub fn watch_task_name(args : Array[String]) -> Result[String, @core.MulpError] {
  if args.length() >= 2 && args[0] == "--watch" {
    Ok(args[1])
  } else {
    Err(@core.config_error("missing watch task"))
  }
}

///|
pub fn mulp_run_watch(
  cwd~ : String,
  args~ : Array[String],
) -> CommandResult {
  let cwd = match command_cwd_from_args(cwd, args) {
    Ok(c) => c
    Err(e) => return { status: 1, stdout: "", stderr: e.message() }
  }
  let args = match args_without_cwd(args) {
    Ok(a) => a
    Err(e) => return { status: 1, stdout: "", stderr: e.message() }
  }
  let script_args = match args_without_config_path(args) {
    Ok(a) => a
    Err(e) => return { status: 1, stdout: "", stderr: e.message() }
  }
  let task_name = match watch_task_name(script_args) {
    Ok(t) => t
    Err(e) => return { status: 1, stdout: "", stderr: e.message() }
  }
  let document = match load_real_config(cwd~, config_path=None) {
    Ok(d) => d
    Err(e) => return { status: 1, stdout: "", stderr: e.message() }
  }
  if not(@cli.is_mulp_script_path(document.path)) {
    return {
      status: 1,
      stdout: "",
      stderr: @core.config_error("watch requires gulp.mbtx").message(),
    }
  }
  let script_path = match discover_moon_root(cwd~) {
    Ok(root) => @cli.path_relative_to_root(document.path, moon_root=root)
    Err(_) => document.path
  }
  let command = @cli.moon_run_script_command(script_path, [task_name])
  let first = execute_moon_run_command(command, cwd)
  let first_output = command_terminal_output(first)
  if first_output.stdout != "" {
    write_stdout(@utf8.encode(first_output.stdout))
  }
  if first_output.stderr != "" {
    write_stderr(@utf8.encode(first_output.stderr))
  }
  register_sigint()
  let opts = @glob_watcher.Options::{
    ..@glob_watcher.Options::default(),
    cwd: Some(cwd),
  }
  let watcher = try {
    @glob_watcher.GlobWatcher::new(
      @chokidar.WatchPaths::One("**/*"),
      opts,
    )
  } catch {
    @glob_watcher.GlobError(msg) | @glob_watcher.SetupError(msg) =>
      return { status: 1, stdout: "", stderr: msg }
  }
  let on_event = fn(_event : @chokidar.Event) -> Unit raise Error {
    let r = execute_moon_run_command(command, cwd)
    let o = command_terminal_output(r)
    if o.stdout != "" {
      write_stdout(@utf8.encode(o.stdout))
    }
    if o.stderr != "" {
      write_stderr(@utf8.encode(o.stderr))
    }
  }
  while check_sigint() == 0 {
    try {
      watcher.tick(cb=on_event)
    } catch {
      @glob_watcher.GlobError(msg) | @glob_watcher.SetupError(msg) => {
        watcher.close()
        return { status: 1, stdout: "", stderr: msg }
      }
    }
    sleep_ms(opts.loop_interval)
  }
  watcher.close()
  { status: 0, stdout: "", stderr: "" }
}

///|
fn command_config_path_from_args(
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
fn command_cwd_from_args(
  cwd : String,
  args : Array[String],
) -> Result[String, @core.MulpError] {
  let mut index = 0
  while index < args.length() {
    if args[index] == "--cwd" {
      if index + 1 >= args.length() {
        return Err(@core.config_error("missing value for --cwd"))
      }
      return Ok(join_config_path(cwd, args[index + 1]))
    }
    index = index + 1
  }
  Ok(cwd)
}

///|
fn args_without_cwd(
  args : Array[String],
) -> Result[Array[String], @core.MulpError] {
  let filtered : Array[String] = []
  let mut index = 0
  while index < args.length() {
    if args[index] == "--cwd" {
      if index + 1 >= args.length() {
        return Err(@core.config_error("missing value for --cwd"))
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
fn args_without_log_flags(args : Array[String]) -> Array[String] {
  let filtered : Array[String] = []
  for arg in args {
    if arg != "--silent" && arg != "--quiet" {
      filtered.push(arg)
    }
  }
  filtered
}

///|
fn log_level_from_args(args : Array[String]) -> Int {
  for arg in args {
    if arg == "--silent" {
      return 0
    }
    if arg == "--quiet" {
      return 1
    }
  }
  2
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
pub fn mulp_command_result_from(
  cwd~ : String,
  args~ : Array[String],
) -> CommandResult {
  if args.length() == 1 && args[0] == "--help" {
    { status: 0, stdout: mulp_command_output(args), stderr: "" }
  } else if args.length() == 1 && (args[0] == "--version" || args[0] == "-v") {
    { status: 0, stdout: mulp_version(), stderr: "" }
  } else {
    let cwd = match command_cwd_from_args(cwd, args) {
      Ok(path) => path
      Err(err) => return { status: 1, stdout: "", stderr: err.message() }
    }
    let args = match args_without_cwd(args) {
      Ok(filtered) => filtered
      Err(err) => return { status: 1, stdout: "", stderr: err.message() }
    }
    let config_path = match command_config_path_from_args(args) {
      Ok(path) => path
      Err(err) => return { status: 1, stdout: "", stderr: err.message() }
    }
    let document = match load_real_config(cwd~, config_path~) {
      Ok(found) => found
      Err(err) => return { status: 1, stdout: "", stderr: err.message() }
    }
    if @cli.is_mulp_script_path(document.path) {
      match @cli.parse_args(args, default_task="") {
        Ok(_) => ()
        Err(err) => return { status: 1, stdout: "", stderr: err.message() }
      }
      let script_args = match args_without_config_path(args) {
        Ok(filtered) => filtered
        Err(err) => return { status: 1, stdout: "", stderr: err.message() }
      }
      let script_path = match discover_moon_root(cwd~) {
        Ok(root) => @cli.path_relative_to_root(document.path, moon_root=root)
        Err(_) => document.path
      }
      let command = @cli.moon_run_script_command(
        script_path,
        normalize_script_args(args_without_log_flags(script_args)),
      )
      return execute_moon_run_command(command, cwd)
    }
    let config = match @cli.parse_config(document.contents) {
      Ok(parsed) => parsed
      Err(err) => return { status: 1, stdout: "", stderr: err.message() }
    }
    let cli = match
      @cli.parse_args(args, default_task=config.mulp.default_task) {
      Ok(parsed) => parsed
      Err(err) => return { status: 1, stdout: "", stderr: err.message() }
    }
    let inferred_entry = match discover_moon_root(cwd~) {
      Ok(root) =>
        Some(@cli.infer_entry_from_mulp_path(document.path, moon_root=root))
      Err(_) => None
    }
    match cli.action {
      @cli.Run(_) | @cli.Watch(_) | @cli.ListTasks | @cli.TasksJson | @cli.Tree(_) =>
        match @cli.moon_run_command_with_entry(config, cli, inferred_entry~) {
          Ok(command) => execute_moon_run_command(command, cwd)
          Err(err) => { status: 1, stdout: "", stderr: err.message() }
        }
    }
  }
}

///|
pub fn mulp_command_output_from(
  cwd~ : String,
  args~ : Array[String],
) -> Result[String, @core.MulpError] {
  let result = mulp_command_result_from(cwd~, args~)
  if result.status == 0 {
    Ok(result.stdout)
  } else {
    Err(@core.config_error(result.stderr))
  }
}

///|
fn args_without_program() -> Array[String] {
  let args = @env.args()
  let rest : Array[String] = []
  for index in 1..<args.length() {
    rest.push(args[index])
  }
  rest
}

///|
fn main {
  let cwd = match @env.current_dir() {
    Some(path) => path
    None => "."
  }
  let args = args_without_program()
  let is_watch = args.iter().any(fn(a) { a == "--watch" })
  if is_watch {
    let result = mulp_run_watch(cwd~, args~)
    if result.status != 0 {
      let output = command_terminal_output(result)
      if output.stderr != "" {
        write_stderr(@utf8.encode(output.stderr))
      }
      exit_process(result.status)
      panic()
    }
  } else {
    let log_level = log_level_from_args(args)
    let result = mulp_command_result_from(cwd~, args~)
    let output = command_terminal_output(result)
    if output.stdout != "" && log_level > 0 {
      write_stdout(@utf8.encode(output.stdout))
    }
    if output.stderr != "" {
      write_stderr(@utf8.encode(output.stderr))
    }
    if result.status != 0 {
      exit_process(result.status)
      panic()
    }
  }
}

///|
#borrow(text)
extern "c" fn write_stdout(text : Bytes) -> Unit = "mulp_write_stdout"

///|
#borrow(text)
extern "c" fn write_stderr(text : Bytes) -> Unit = "mulp_write_stderr"

///|
extern "c" fn exit_process(status : Int) -> Unit = "mulp_exit_process"

///|
extern "c" fn register_sigint() -> Unit = "mulp_register_sigint"

///|
extern "c" fn check_sigint() -> Int = "mulp_check_sigint"

///|
extern "c" fn sleep_ms(ms : Int) -> Unit = "mulp_sleep_ms"
