# i5ting/chokidar

Minimal and efficient cross-platform file watching library for MoonBit (native target).

A faithful port of [chokidar](https://github.com/paulmillr/chokidar) v5 to MoonBit, with the same API shape and identical option names (in MoonBit snake\_case convention). Runs on macOS (kqueue), Linux (inotify), and FreeBSD with no external dependencies.

## Getting started

```bash
moon add i5ting/chokidar
```

```moonbit
fn main {
  let watcher = @chokidar.watch(@chokidar.One("."))
  watcher
    .on(@chokidar.Add,    event => println("added:   " + event.path))
    .on(@chokidar.Change, event => println("changed: " + event.path))
    .on(@chokidar.Unlink, event => println("removed: " + event.path))
    .on(@chokidar.Ready,  _     => println("ready"))
  |> ignore

  // Drive the watcher in your application loop
  while true {
    watcher.process() catch { _ => () }
  }
}
```

## How it works

Unlike the Node.js original, the MoonBit port is **poll-driven**: no background threads are spawned. You call `watcher.process()` in your own loop to drain OS events and run diff scans. Between calls the watcher is completely idle.

Native OS notifications are used for low latency (kqueue on macOS/FreeBSD, inotify on Linux). Polling mode (`use_polling: true`) falls back to periodic directory snapshots.

## API

### `watch`

```moonbit
pub fn watch(
  paths : WatchPaths,
  options? : Options,
) -> Watcher raise ChokidarError
```

`WatchPaths` is either `One(String)` or `Many(Array[String])`.

Returns a `Watcher`. Raises `ChokidarError` if an initial scan fails.

### `Watcher`

| Method | Description |
|--------|-------------|
| `add(paths) -> Watcher` | Add more paths to watch |
| `unwatch(paths) -> Watcher` | Stop watching paths |
| `on(event, callback) -> Watcher` | Subscribe to an event |
| `off(event, callback) -> Watcher` | Unsubscribe a specific callback |
| `remove_all_listeners() -> Watcher` | Remove all event callbacks |
| `get_watched() -> Map[String, Array[String]]` | Return currently watched paths |
| `close() -> Unit` | Stop all watchers and clear state |
| `process() -> Unit raise ChokidarError` | Drain OS events and emit file-change events |

### Events

| `EventName` | Emitted when |
|-------------|--------------|
| `Add` | A file is added |
| `AddDir` | A directory is added |
| `Change` | A file changes |
| `Unlink` | A file is removed |
| `UnlinkDir` | A directory is removed |
| `Ready` | Initial scan complete (emitted once) |
| `Raw` | Raw OS event received |
| `Error` | A scan error occurs |
| `All` | Any of Add/AddDir/Change/Unlink/UnlinkDir |

Each callback receives an `Event`:

```moonbit
pub struct Event {
  name  : EventName
  path  : String
  stats : FileStats?      // size, mode, modified_nanos
  raw   : RawEvent?       // underlying OS event (Raw only)
  error : ChokidarError?  // (Error only)
}
```

`FileStats` is attached when `always_stat: true` or on `Change` events:

```moonbit
pub struct FileStats {
  size           : Int64
  mode           : Int
  modified_nanos : Int64
}
```

`FileKind` classifies the entry type. Special files (sockets, FIFOs, device nodes) use the `Other` variant and generate `Add`/`Unlink` events:

```moonbit
pub enum FileKind {
  File
  Directory
  Symlink
  Other   // sockets, pipes, device nodes
}
```

### Options

```moonbit
let watcher = @chokidar.watch(
  @chokidar.One("src"),
  options={
    ..@chokidar.Options::default(),
    ignored: [
      // exact path
      @chokidar.MatchPath("src/generated.mbt"),
      // recursive subtree
      @chokidar.MatchRecursivePath("src/vendor"),
      // regex
      @chokidar.MatchRegex(@regexp.compile("\\.log$") |> Result::unwrap),
      // function
      @chokidar.MatchFunction((path, _stats) => path.has_suffix(".tmp")),
    ],
    ignore_initial: true,
    follow_symlinks: true,
    cwd: Some("/project/root"),
    use_polling: false,
    interval: 100,
    binary_interval: 300,
    always_stat: false,
    depth: 2147483647,      // Int::max_value — unlimited
    ignore_permission_errors: false,
    atomic_option: @chokidar.AtomicDefault,
    await_write_finish: @chokidar.AwaitWriteFinishOff,
  }
)
```

#### Path filtering

- **`ignored`** — Array of `Matcher` values; a path is ignored if any matcher matches.
  - `MatchPath(s)` — exact path equality
  - `MatchRecursivePath(s)` — `s` or any path underneath `s`
  - `MatchRegex(re)` — matches via `@regexp.Regexp`
  - `MatchFunction((path, stats?) -> Bool)` — custom predicate
- **`ignore_initial`** (default `false`) — suppress `add`/`addDir` events during the initial scan.
- **`follow_symlinks`** (default `true`) — traverse symlinked directories.
- **`cwd`** — base directory; emitted paths will be relative to it.

#### Performance

- **`use_polling`** (default `false`) — force directory polling instead of native OS events. Required for network filesystems. Also overridable via `CHOKIDAR_USEPOLLING=true|1|false|0`.
- **`interval`** (default `100` ms) — polling interval. Also overridable via `CHOKIDAR_INTERVAL=<ms>`.
- **`binary_interval`** (default `300` ms) — polling interval for binary files (option accepted; uniform interval used for all files).
- **`always_stat`** (default `false`) — always attach `FileStats` to `Add`, `AddDir`, and `Change` events.
- **`depth`** (default unlimited) — maximum directory depth to scan.

#### Correctness

- **`ignore_permission_errors`** (default `false`) — silently skip unreadable files instead of raising.
- **`atomic_option`**:
  - `AtomicDefault` — detect editor atomic-write patterns (vim swapfiles, `~` backups, Sublime `.tmp`); suppress the intermediate `Unlink` and convert `Unlink`+`Add` pairs to `Change`.
  - `AtomicWindow(ms)` — explicit window in milliseconds.
  - `AtomicOff` — disable atomic handling.
- **`await_write_finish`**:
  - `AwaitWriteFinishOff` (default) — emit immediately.
  - `AwaitWriteFinishOn({ stability_threshold: Int, poll_interval: Int })` — hold `Add`/`Change` until file size is stable for `stability_threshold` ms; re-check every `poll_interval` ms.
  - `AwaitWriteFinish::default_on()` — `{ stability_threshold: 2000, poll_interval: 100 }`.

## Differences from chokidar-main (Node.js v5)

| Topic | chokidar-main (JS) | i5ting/chokidar (MoonBit) |
|-------|-------------------|---------------------------|
| Event model | Async callbacks via EventEmitter | Synchronous; call `watcher.process()` in a loop |
| `close()` | Returns `Promise<void>` | Synchronous `Unit` |
| `on` / `off` return type | `FSWatcher` (this) | `Watcher` (same behavior) |
| Event names | string literals: `"add"`, `"addDir"` … | `EventName` enum variants; `all_event_names()` lists all nine |
| Matcher types | string, RegExp, function, `{ path, recursive? }` | `Matcher` enum: `MatchPath`, `MatchRecursivePath`, `MatchRegex`, `MatchFunction` |
| `ignored` + `cwd` | `MatchPath` patterns are joined with `cwd` so absolute patterns work | `MatchPath` uses literal string equality; combine `cwd` into the pattern manually |
| Non-existent watch paths | Silently deferred until path appears | Raises `ChokidarError` on first scan |
| `binaryInterval` | Different poll interval for binary file extensions | Option accepted; uniform interval used |
| Special files | socket/FIFO/device nodes counted separately | Covered by `FileKind::Other`; emit `Add`/`Unlink` like regular files |

### Name mapping (JS → MoonBit)

| JS name | MoonBit name | Note |
|---------|-------------|------|
| `FSWatcher` | `Watcher` | |
| `watch()` | `watch()` | Same |
| `FSWatcher.add()` | `Watcher::add()` | Same |
| `FSWatcher.on()` | `Watcher::on()` | Same |
| `FSWatcher.off()` | `Watcher::off()` | Same |
| `FSWatcher.unwatch()` | `Watcher::unwatch()` | Same |
| `FSWatcher.close()` | `Watcher::close()` | Sync in MoonBit |
| `FSWatcher.getWatched()` | `Watcher::get_watched()` | snake\_case |
| `FSWatcher.removeAllListeners()` | `Watcher::remove_all_listeners()` | snake\_case |
| `ignoreInitial` | `ignore_initial` | snake\_case |
| `followSymlinks` | `follow_symlinks` | snake\_case |
| `usePolling` | `use_polling` | snake\_case |
| `binaryInterval` | `binary_interval` | snake\_case |
| `alwaysStat` | `always_stat` | snake\_case |
| `ignorePermissionErrors` | `ignore_permission_errors` | snake\_case |
| `awaitWriteFinish` | `await_write_finish` | snake\_case |
| `atomic` | `atomic_option` | renamed (avoids keyword conflict) |

## Examples

### Watch a directory, ignore node\_modules

```moonbit
let watcher = @chokidar.watch(
  @chokidar.One("src"),
  options={
    ..@chokidar.Options::default(),
    ignore_initial: true,
    ignored: [@chokidar.MatchRecursivePath("src/node_modules")],
  },
)
watcher.on(@chokidar.All, event => {
  println(event.name.to_string() + " " + event.path)
}) |> ignore
```

### Watch multiple paths

```moonbit
let watcher = @chokidar.watch(
  @chokidar.Many(["src", "test"]),
  options={
    ..@chokidar.Options::default(),
    ignore_initial: true,
  },
)
```

### Await write finish for large files

```moonbit
let watcher = @chokidar.watch(
  @chokidar.One("uploads"),
  options={
    ..@chokidar.Options::default(),
    await_write_finish: @chokidar.AwaitWriteFinish::default_on(),
  },
)
```

### Force polling (e.g. for network drives)

```moonbit
let watcher = @chokidar.watch(
  @chokidar.One("/mnt/share"),
  options={
    ..@chokidar.Options::default(),
    use_polling: true,
    interval: 500,
  },
)
```

### Attach stats to every change event

```moonbit
watcher.on(@chokidar.Change, event => {
  match event.stats {
    Some(s) => println("size: " + s.size.to_string())
    None => ()
  }
}) |> ignore
```

### Unsubscribe a listener

```moonbit
let cb : (@chokidar.Event) -> Unit = event => println(event.path)
watcher.on(@chokidar.Add, cb) |> ignore
// later...
watcher.off(@chokidar.Add, cb) |> ignore
```

### Stop watching

```moonbit
watcher.close()
```

## Environment variable overrides

| Variable | Effect |
|----------|--------|
| `CHOKIDAR_USEPOLLING=true` \| `1` | Force `use_polling: true` |
| `CHOKIDAR_USEPOLLING=false` \| `0` | Force `use_polling: false` |
| `CHOKIDAR_INTERVAL=<ms>` | Override `interval` |

## License

Apache-2.0
