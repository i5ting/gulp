///|
/// Build-trigger demo: debounced watcher that simulates a rebuild on each
/// TypeScript/JavaScript change under src/.
///
/// Run with: moon run cmd/rebuild --target native
fn main {
  try {
    rebuild_loop()
  } catch {
    @glob_watcher.GlobError(msg)  => println("[rebuild] error: invalid glob — " + msg)
    @glob_watcher.SetupError(msg) => println("[rebuild] error: watcher setup — " + msg)
  }
}

fn rebuild_loop() -> Unit raise @glob_watcher.GlobWatcherError {
  // Debounce 300 ms; queue the next event so a slow build is never overlapped.
  let opts = @glob_watcher.Options::{
    ..@glob_watcher.Options::default(),
    delay:          300,
    queue:          true,
    ignore_initial: true,
    use_polling:    false,
  }

  let w = @glob_watcher.GlobWatcher::new(
    @chokidar.WatchPaths::Many(["src/**/*.ts", "src/**/*.js", "!**/*.d.ts"]),
    opts,
  )

  w.on(@chokidar.EventName::Ready, _ => println("[rebuild] watching src/ …"))
   |> ignore

  w.run(cb=event => {
    println("[rebuild] " + event.name.to_string() + " " + event.path)
    println("[rebuild] building …")
    // In a real tool: invoke moon build / tsc / esbuild via FFI here.
    println("[rebuild] done")
  })
}
