///|
extern "C" fn cli_init() -> Unit = "chokidar_cli_init"

///|
extern "C" fn cli_sleep_ms(ms : Int) -> Unit = "chokidar_cli_sleep_ms"

///|
fn main {
  cli_init()
  let cmd = @argparse.Command::new(
    "chokidar",
    about="Watch files and directories for changes.",
    version=@lib.version(),
    positionals=[
      @argparse.PositionArg::new(
        "paths",
        about="Paths to watch (default: .)",
        num_args=@argparse.ValueRange(lower=0),
        default_values=["."],
      ),
    ],
  )
  let matches = cmd.parse() catch {
    err => {
      println(err.to_string())
      return
    }
  }
  let paths = match matches.values.get("paths") {
    Some(ps) => ps
    None => ["."]
  }
  let watch_paths = if paths.length() == 1 {
    @lib.One(paths[0])
  } else {
    @lib.Many(paths)
  }
  let watcher = @lib.watch(watch_paths, options={
    ..@lib.Options::default(),
    use_polling: true,
  }) catch {
    @lib.ChokidarError(msg) => {
      println("error: " + msg)
      return
    }
  }
  watcher
  .on(@lib.All, event => {
    if event.name != @lib.Ready && event.name != @lib.Raw {
      println(event.name.to_string() + " " + event.path)
    }
  })
  .on(@lib.Ready, _ => println("Ready. Watching for changes..."))
  |> ignore
  while true {
    watcher.process() catch {
      @lib.ChokidarError(msg) => println("error: " + msg)
    }
    cli_sleep_ms(100)
  }
}
