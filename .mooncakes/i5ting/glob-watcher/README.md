# i5ting/glob-watcher

Glob-pattern file watcher for MoonBit native — debounced, queue-aware, chokidar-backed.

## Features

- **Glob patterns** — `**/*.ts`, `src/**/*.{js,ts}`, negations `!vendor/**`, brace expansion
- **Debouncing** — coalesces rapid bursts into a single callback invocation
- **Queue-aware** — optionally queues the next event while a callback is busy
- **Event filtering** — watch only `Add`, `Change`, `Unlink`, or any combination
- **Lifecycle hooks** — register `Ready`, `Error`, `Raw` listeners via `.on()`
- **Native target only** — requires `--target native` (or `llvm`)

## Installation

```bash
moon add i5ting/glob-watcher
```

Because the package name contains a hyphen, set an explicit alias in `moon.pkg`:

```
import {
  "i5ting/glob-watcher" @glob_watcher,
  "i5ting/chokidar",
}
options(
  "warn-list": "-29",
)
```

Then use `@glob_watcher.` in your source files.

## Quick start

```moonbit
fn main {
  try {
    @glob_watcher.watch(
      @chokidar.WatchPaths::One("**/*.{js,ts}"),
      @glob_watcher.Options::default(),
      event => println(event.name.to_string() + " " + event.path),
    )
  } catch {
    @glob_watcher.GlobError(msg)  => println("glob error: "  + msg)
    @glob_watcher.SetupError(msg) => println("setup error: " + msg)
  }
}
```

`watch` blocks until SIGINT/SIGTERM.

## More examples

### Watch with custom options

```moonbit
fn run() -> Unit raise @glob_watcher.GlobWatcherError {
  let opts = @glob_watcher.Options::{
    ..@glob_watcher.Options::default(),
    delay:          300,          // ms to debounce
    queue:          true,         // queue events while callback runs
    ignore_initial: true,         // skip pre-existing files
    use_polling:    false,        // use native fs events (kqueue/inotify)
    cwd:            Some("src"),  // resolve relative globs from here
  }

  let w = @glob_watcher.GlobWatcher::new(
    @chokidar.WatchPaths::Many(["**/*.ts", "!**/*.d.ts"]),
    opts,
  )

  w.on(@chokidar.EventName::Ready, _ => println("ready"))
   |> ignore

  w.run(cb=event =>
    println(event.name.to_string() + " " + event.path)
  )
}
```

### Watch only specific event types

```moonbit
fn watch_adds_only() -> Unit raise @glob_watcher.GlobWatcherError {
  let opts = @glob_watcher.Options::{
    ..@glob_watcher.Options::default(),
    events: [@chokidar.EventName::Add],  // fire only on new files
  }
  @glob_watcher.watch(
    @chokidar.WatchPaths::One("uploads/**"),
    opts,
    event => println("new file: " + event.path),
  )
}
```

### Custom loop with `tick`

Use `tick` when you need to interleave watching with other work:

```moonbit
fn custom_loop(w : @glob_watcher.GlobWatcher) -> Unit raise @glob_watcher.GlobWatcherError {
  loop {
    // drive one debounce+queue cycle
    w.tick(cb=event => println("changed: " + event.path))
    // ... do other work here ...
  }
}
```

### Inspect watched paths

```moonbit
fn show_watched(w : @glob_watcher.GlobWatcher) -> Unit {
  let dirs = w.get_watched()
  for dir, files in dirs {
    println(dir + ":")
    for f in files {
      println("  " + f)
    }
  }
}
```

## Options

| Field | Type | Default | Description |
|---|---|---|---|
| `delay` | `Int` | `200` | Debounce window in ms |
| `queue` | `Bool` | `true` | Queue next event while callback is busy |
| `events` | `Array[EventName]` | `[Add, Change, Unlink]` | Event types that trigger the callback |
| `ignore_initial` | `Bool` | `true` | Skip files that exist before watch starts |
| `ignored` | `Array[Matcher]` | `[]` | Extra ignore matchers passed to chokidar |
| `cwd` | `String?` | `None` | Working directory for relative glob paths |
| `depth` | `Int` | `Int::max_value` | Maximum directory depth |
| `loop_interval` | `Int` | `10` | Polling interval for the run loop in ms |
| `use_polling` | `Bool` | `true` | Use polling instead of native fs events |

## Debounce semantics

Within a debounce window, each new event **replaces** the pending one. When the window closes, the callback fires exactly once with the most-recent event. This is the correct behaviour for "trigger a rebuild" use cases — the build tool (webpack, esbuild, tsc) performs its own incremental scan and does not depend on the watcher enumerating every changed path.

If you need to react to multiple simultaneous changes, decrease `delay` or use `tick()` in a custom loop where you post-process `file_events` yourself.

## Glob syntax

| Pattern | Matches |
|---|---|
| `*.ts` | Any `.ts` file in the root |
| `**/*.ts` | Any `.ts` file at any depth |
| `src/**/*.{js,ts}` | `.js` or `.ts` files under `src/` |
| `!vendor/**` | Exclude anything under `vendor/` |

Negation patterns (`!`) are converted to chokidar `ignored` matchers.
A later positive that matches a negation cancels the negation (re-add semantics).

### `cwd` and event paths

When `cwd` is set, `event.path` in the callback is **relative to `cwd`** (e.g. `src/index.ts`).
When `cwd` is `None`, `event.path` reflects the path as seen by the OS (relative to the process working directory).

`cwd` also sets the base for relative glob patterns: `cwd = Some("src")` + `"**/*.ts"` watches the `src/` directory and reports paths like `util/parser.ts`.

## API

### `watch`

```moonbit
pub fn watch(
  glob    : @chokidar.WatchPaths,
  options : Options,
  cb      : (@chokidar.Event) -> Unit raise Error,
) -> Unit raise GlobWatcherError
```

High-level entry point. Blocks until the process exits.

### `GlobWatcher::new`

```moonbit
pub fn GlobWatcher::new(
  glob    : @chokidar.WatchPaths,
  options : Options,
) -> GlobWatcher raise GlobWatcherError
```

Create a watcher without starting the loop.

### `GlobWatcher::on`

```moonbit
pub fn GlobWatcher::on(
  self     : GlobWatcher,
  event    : @chokidar.EventName,
  callback : (@chokidar.Event) -> Unit,
) -> GlobWatcher
```

Register a listener for `Ready`, `Error`, `Raw`, `All`, etc. Returns `self` for chaining.

### `GlobWatcher::run`

```moonbit
pub fn GlobWatcher::run(
  self : GlobWatcher,
  cb?  : (@chokidar.Event) -> Unit raise Error,
) -> Unit raise GlobWatcherError
```

Block in a polling loop. Pass `cb` to receive debounced file events.

### `GlobWatcher::tick`

```moonbit
pub fn GlobWatcher::tick(
  self : GlobWatcher,
  cb?  : (@chokidar.Event) -> Unit raise Error,
) -> Unit raise GlobWatcherError
```

Drive one process + debounce + queue cycle without blocking. Useful for tests and custom loop implementations.

### `GlobWatcher::close`

```moonbit
pub fn GlobWatcher::close(self : GlobWatcher) -> Unit
```

Stop watching and release resources.

### `GlobWatcher::get_watched`

```moonbit
pub fn GlobWatcher::get_watched(
  self : GlobWatcher,
) -> Map[String, Array[String]]
```

Return a snapshot of the currently watched directories and their files.

## Errors

```moonbit
pub(all) suberror GlobWatcherError {
  GlobError(String)    // invalid or negation-only glob patterns
  SetupError(String)   // chokidar initialisation failure
}
```

## Running the examples

```bash
# basic demo — watch **/*.{js,ts} and print each change
moon run cmd/main --target native

# build-trigger demo — watch src/**/*.ts and simulate a rebuild
moon run cmd/rebuild --target native
```

## Building

```bash
moon build --target native
moon test  --target native
```
