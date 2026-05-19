///|
/// Demo: watch **/*.{js,ts} in the current directory and print each change.
/// Run with `moon run cmd/main --target native` from the project root.
fn main {
  try {
    run()
  } catch {
    @globwatcher.GlobError(msg) => println("glob error: " + msg)
    @globwatcher.SetupError(msg) => println("setup error: " + msg)
  }
}

fn run() -> Unit raise @globwatcher.GlobWatcherError {
  let opts = @globwatcher.Options::{
    ..@globwatcher.Options::default(),
    delay:          200,
    ignore_initial: true,
    use_polling:    false,
  }
  let w = @globwatcher.GlobWatcher::new(
    @chokidar.WatchPaths::One("**/*.{js,ts}"),
    opts,
  )
  w.on(@chokidar.EventName::Ready, _ => println("watching **/*.{js,ts} ...")) |> ignore
  w.run(cb=event => println(event.name.to_string() + " " + event.path))
}
