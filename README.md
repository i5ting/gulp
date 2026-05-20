# gulp

`gulp` is a MoonBit-native build tool inspired by the Node.js gulp task runner.

This repository slice currently contains:

- `i5ting/gulp/core`: task registration, task lookup, `series`, `parallel`, `lastRun`,
  and registry tree output
- `i5ting/gulp/stream`: byte streams, file streams, zero-to-many transforms, and
  in-memory `src`/`dest`, plus native file-backed `ByteStream` and `file_dest`
  for files, empty files, directories, symlinks, filesystem metadata, and named
  stream plugins; context-aware file byte streams close native readers on
  cancellation
- `i5ting/gulp/vinyl`: a MoonBit Vinyl file model with contents, path history,
  stat kind, symlink target, custom metadata, clone with replayable stream
  contents, base/cwd proxy behavior, and path-derived helpers
- `i5ting/gulp/through2`: typed MoonBit-native through2-style transforms for
  `ByteStream`, `FileStream`, and `VinylFileStream`, covering passthrough,
  map/filter/flatMap, flush, error propagation, and reusable factories
- `i5ting/gulp/cli`: pure config parsing, CLI argument selection, task list/tree
  rendering, and `moon run` proxy command modeling
- `i5ting/gulp/entry`: the task-entry convention for user build packages
- `i5ting/gulp/platform`: in-memory filesystem behavior for platform-layer tests,
  including file reads/writes, directory creation, deletion, stat, path
  normalization, and native `moon run` process execution with stdout/stderr
  capture, clocks, signal polling, cancellation bridging, glob expansion, and
  lazy glob discovery, plus glob watch snapshots for added/removed path events
  and explicit macOS/Linux/Windows path behavior tests
- `i5ting/gulp/cmd/gulp`: native executable entry package with a minimal command
  surface, real upward `gulp.mbtx` discovery, and explicit script path loading
  that proxies default and explicit tasks into `moon run` commands
  and can list/render configured tasks with structured command status
- `i5ting/gulp/examples/basic`: a minimal `gulp.mbtx` task script

Full filesystem-backed `src`, remaining CLI actions, watch mode, and broader OS
backends have their own implementation slices.

## Current Usage

This slice can be used from MoonBit package tests and the native `gulp`
command package. The native command reads `gulp.mbtx` from the real filesystem
and dispatches configured tasks through `moon run`.

### Quick Start

Create a `gulp.mbtx` file at your project root:

```moonbit
///|
import {
  "moonbitlang/core/env" @env,
  "i5ting/gulp/entry" @entry,
}

///|
fn build(_ctx : @core.Context) -> Result[Unit, @core.MulpError] {
  println("building...")
  Ok(())
}

///|
fn main {
  @entry.run_tasks([("build", build)], @env.args(), default_task="build")
}
```

Then run:

```bash
gulp
gulp build
gulp --tasks
gulp --tasks-simple
gulp --tree build
gulp --watch build
```

`gulp` searches upward from the current directory for `gulp.mbtx`. You can also
point to a script explicitly:

```bash
gulp --gulpfile path/to/gulp.mbtx build
gulp --file path/to/gulp.mbtx --tasks
gulp --gulpfile path/to/gulp.mbtx --tree build
```

For local repository development, run the native command package directly:

```bash
moon run cmd/gulp --target native -- --cwd examples/basic build
```

### Native Release Smoke

Build the native executable in release mode:

```bash
cd gulp
moon build cmd/gulp --release --target native
```

The release binary is emitted at:

```text
gulp/_build/native/release/build/cmd/gulp/gulp.exe
```

Install it by copying that file to a directory on `PATH`, renaming it to `gulp`
on macOS/Linux if desired:

```bash
mkdir -p ~/.local/bin
cp gulp/_build/native/release/build/cmd/gulp/gulp.exe ~/.local/bin/gulp
chmod +x ~/.local/bin/gulp
export PATH="$HOME/.local/bin:$PATH"
```

Run the release smoke test to verify the binary can be found through `PATH` and
can execute the basic example end to end:

```bash
node gulp/scripts/release_smoke.mjs
```

The CLI validates `gulp` flags, then proxies the remaining task arguments to:

```bash
moon run --target native <gulp.mbtx> -- <args>
```

Bare discovery does not fall back to `gulp.mbt` or `gulp.toml`. Legacy
`gulp.mbt` metadata can still be loaded through an explicit path for
compatibility tests.

### Exit Codes

The native command keeps its status source explicit:

| Case | Status | Stream |
| --- | ---: | --- |
| `--help`, `--version`, `-v`, successful task, successful `--tasks` / `--tasks-simple` / `-T` / `--tree` | `0` | stdout |
| CLI usage error, missing `gulp.mbtx`, unreadable config, malformed legacy config | `1` | stderr |
| Unknown task reported by `gulp.mbtx` | script status, usually `1` | script stderr |
| `--watch <task>` where the script task fails | script status, usually `1` | script stderr |
| Explicit legacy entry or `moon run` process failure | child process status | child stdout/stderr |

### Core Tasks

Register tasks in a `Registry`, then compose them with `series` or `parallel`:

```moonbit
let registry = new_registry()
let build = task("build", fn(ctx) -> Result[Unit, MulpError] {
  if ctx.is_cancelled() {
    Err(task_failed("build", "cancelled"))
  } else {
    Ok(())
  }
})

registry.register(build).unwrap()
let pipeline = series([build])
pipeline.run(new_context(
  cwd="/workspace",
  now_ms=0L,
  cancellation=new_cancellation_token(),
))
```

### In-Memory File Pipelines

Use `src` to create an in-memory `FileStream`, compose zero-to-many transforms
with `pipe`, and terminate with `dest`:

```moonbit
let input = file(
  cwd="/workspace",
  base="/workspace/src",
  path="/workspace/src/app.txt",
  contents=Bytes(byte_stream(["hello"])),
)

let sink = memory_dest()
let pipeline = src([input])
  .pipe(fn(source : File) -> FileStream {
    let copy = file(
      cwd=source.cwd,
      base=source.base,
      path=source.path + ".copy",
      contents=source.contents,
    )
    file_stream([source, copy])
  })
  .pipe(dest("dist", sink))

let ctx = new_context(
  cwd="/workspace",
  now_ms=0L,
  cancellation=new_cancellation_token(),
)
let outputs = pipeline.collect(ctx).unwrap()
```

`dest` records output files and byte contents in `MemoryDest`:

```moonbit
inspect(outputs.length(), content="2")
inspect(sink.contents_for("dist/app.txt").unwrap(), content="hello")
```

`File::relative_path()` preserves paths under `base` and falls back to the full
path for files outside that base. Use `Text("...")` for text contents,
`Buffer(bytes)` for native byte buffers, and `Bytes(stream)` for streaming byte
contents. `ByteStream` reads `Bytes` chunks and is single-consumption, so a
file's byte contents should be read by one terminal consumer. Use
`byte_stream_from_bytes` for already-buffered bytes, `read_all_bytes(max_bytes=...)`
when raw bytes are needed, and `read_all(max_bytes=...)` when a bounded text
string is needed.

For plugin logic that would use `through2` in gulp, prefer
`i5ting/gulp/through2` and pipe typed transforms directly:

```moonbit
let rename = @through2.object_transform_files(
  map=Some(fn(file : @stream.File) -> Result[@stream.File?, @core.MulpError] {
    Ok(Some(file.with_path(file.path + ".out")))
  }),
)

let output = @stream.file_stream([input]).pipe_stream(rename)
```

Named plugins are wrappers around zero-to-many transforms and can be piped like
regular transforms when plugin metadata is useful:

```moonbit
let copy_plugin = plugin("copy", fn(file : File) -> FileStream {
  file_stream([file])
})
let output = src([input]).pipe_plugin(copy_plugin)
```

Transforms and plugins can return `error_stream(err)` to propagate failures
through downstream collection or destinations.
Use `file_byte_stream_with_context` when a native file reader should close as
soon as its task context is cancelled.
Native `file_dest` writes byte streams chunk by chunk, so outputs larger than the
in-memory `read_all` limit do not need to be buffered as one string.
Use `merge_sources([a, b])` to concatenate multiple source streams while
preserving stream order and error propagation.
`filter_files`, `map_files`, `flat_map_files`, and `i5ting/gulp/through2`
cover the common zero/one/many transform helpers used by gulp-style plugins.
Use `concat_plugin("bundle.txt")` as a stream-level plugin when multiple
buffered input files should become one output file:

```moonbit
let bundle = src([a, b]).pipe_stream(concat_plugin("bundle.txt"))
```

Native async task integration lives in `i5ting/gulp/stream_async`. Its
`async_task_from_stream(name, stream, signals=...)` adapter drains a `FileStream`
and uses the drain result as the task completion signal. The adapter registers
that stream completion signal with the shared completion validator, so tasks
that also declare callback or process completion fail before draining.
In `i5ting/gulp/core`, `new_async_registry()`, `async_task(...)`,
`async_task_from(...)`, `async_series(...)`, and `async_parallel(...)` provide
the native async task graph. `i5ting/gulp/entry` mirrors those helpers with
`new_async_registry()`, `async_task(...)`, `async_task_from(...)`,
`async_run_args(...)`, `render_async_args(...)`, and
`async_run_entry_args(...)` so custom entry wiring can run async tasks, render
task lists/trees, and fall back to the default `build` task without a hand-written
argument dispatcher.
In `i5ting/gulp/core`, `async_task_from_callback(name, register)` supports an explicit
callback-style completion protocol, waits until the callback is called, and
rejects duplicate callbacks.
`validate_async_completion_signals(name, signals)` rejects tasks that combine
completion modes, such as returning a stream while also calling a callback.
Native child process task integration lives in `i5ting/gulp/platform_async`.
`async_task_from_process(name, command)` spawns a child process, awaits its exit
code, and cancels the child when the shared task context is cancelled.
`async_watch_loop(...)` connects normalized glob watch events to a debounced
async loop, runs the current handler without blocking event polling, queues a
pending rebuild when changes arrive mid-run, and exits cleanly when the shared
context is cancelled.
`async_file_worker_pool(concurrency=...)` provides bounded file-level async
work for future `AsyncFileStream` integrations: it pulls from upstream only
after a worker slot is available, so file processing can be parallelized without
unboundedly reading the stream ahead.

Task concurrency and stream concurrency are separate layers. `series` and
`parallel` schedule whole tasks; a task that drains a `FileStream` still observes
the stream's pull-based backpressure and processes files through the current
sync transform chain. File-level async transforms build on the bounded
`stream_async` worker pool so task-level parallelism does not accidentally
create unbounded file work.

Stream benchmarks cover buffered pipeline throughput and large byte-stream
`file_dest` writes:

```bash
moon bench --package stream --target native
```

Vinyl and vinyl-fs benchmarks are native-only. They cover Vinyl construction,
relative path access, cloning, buffered `src`, buffered `dest`, and streaming
`dest`:

```bash
moon bench --package vinyl --target native
moon bench --package vinyl_fs --target native
```

Use the comparison harness to run the same release-native gulp scenarios next
to local JS scenarios with repeated medians. The harness exits non-zero when
MoonBit native is slower than JS for any selected scenario:

```bash
node gulp/scripts/bench_compare.mjs
node gulp/scripts/bench_compare.mjs --json
node gulp/scripts/bench_compare.mjs --scenario vinyl-fs-src-buffer-1000 --rounds 3
node gulp/scripts/bench_compare.mjs --scenario pipeline-memory-dest --rounds 3
```

### CLI Script Model

`gulp.mbtx` is the user-facing task entry. The CLI discovers it upward from the
current directory or accepts an explicit path through `--config`, `--gulpfile`,
`--file`, or `--gulpfile`, then proxies the original task arguments to MoonBit:

```moonbit
let command = moon_run_script_command("/workspace/app/gulp.mbtx", ["build"])
```

`command.executable` is `"moon"` and `command.args` is
`["run", "--target", "native", "/workspace/app/gulp.mbtx", "--", "build"]`.

Legacy `gulp.mbt` metadata files can still be loaded through an explicit path
for compatibility tests, where `parse_config` and `moon_run_command` infer or
read the older entry package shape.

For in-process task listing and tree rendering, render output from a `Registry`:

```moonbit
let listing = render_cli_output(
  parse_args(["--tasks"], default_task="build").unwrap(),
  registry,
).unwrap()
```

To test config discovery and command dispatch without platform IO, pass a virtual
set of config files:

```moonbit
let result = dispatch_cli(
  cwd="/workspace/app/src",
  files=[("/workspace/app/gulp.mbtx", "fn main { println(\"build\") }")],
  args=["build"],
  registry=registry,
).unwrap()
```

The native command package discovers a real `gulp.mbtx` upward from the current
directory, supports `--config`, `--gulpfile`, `--file`, `--tasks`, and `--tree`,
and runs task scripts as:

```bash
moon run --target native <gulp.mbtx> -- <task>
```

For example, this `gulp.mbtx` dispatches `gulp build` directly:

```moonbit
import {
  "moonbitlang/core/env" @env
}

fn selected_task(args : Array[String]) -> String {
  if args.length() >= 2 {
    args[1]
  } else {
    "build"
  }
}

fn main {
  println("running " + selected_task(@env.args()))
}
```

Discovery does not automatically fall back to legacy config files. Projects
that still need the legacy parser can pass an explicit path with `--config`,
`--gulpfile`, or `--file`; bare `gulp` searches for `gulp.mbtx` only.

### Example Project

A minimal project keeps `gulp.mbtx` at the workspace root:

```moonbit
///|
import {
  "moonbitlang/core/env" @env,
  "i5ting/gulp/entry" @entry,
}

///|
fn build(_ctx : @core.Context) -> Result[Unit, @core.MulpError] { Ok(()) }
fn clean(_ctx : @core.Context) -> Result[Unit, @core.MulpError] { Ok(()) }
fn watch(_ctx : @core.Context) -> Result[Unit, @core.MulpError] { Ok(()) }
fn parallel_task(_ctx : @core.Context) -> Result[Unit, @core.MulpError] { Ok(()) }

///|
fn main {
  @entry.run_tasks(
    [("build", build), ("clean", clean), ("watch", watch), ("parallel", parallel_task)],
    @env.args(),
  )
}
```

A file pipeline follows gulp's `src().pipe(...).pipe(dest())` shape:

```moonbit
let pipeline = @stream.src([input])
  .pipe(@stream.map_files(fn(file : @stream.File) -> @stream.File {
    {
      cwd: file.cwd,
      base: file.base,
      path: file.path + ".copy",
      contents: file.contents,
      metadata: file.metadata,
    }
  }))
  .pipe(@stream.file_dest("dist"))
```

For `series`/`parallel` composition, use the registry API to get task handles:

```moonbit
fn main {
  let registry = @entry.new_registry()
  let clean = @entry.task(registry, "clean", fn(_ctx) -> Result[Unit, @core.MulpError] {
    Ok(())
  })
  let build = @entry.task(registry, "build", fn(_ctx) -> Result[Unit, @core.MulpError] {
    Ok(())
  })
  let all = @core.series([clean, build])
  ignore(@core.task(registry, "default", fn(ctx) { all.run(ctx) }))
  @entry.run(registry, @env.args())
}
```

Watch is invoked through the CLI:

```bash
gulp --watch build
```

Current watch support proxies the watch request to the script and runs the task
once; the long-running native filesystem watcher is tracked in the remaining
watch TODOs.

Key differences from gulp today:

- `gulp.mbtx` is MoonBit source, not JavaScript.
- Node gulp plugins cannot be reused directly.
- Streams are native MoonBit `FileStream` / `ByteStream` values.
- Bare discovery searches for `gulp.mbtx`; legacy config files require an explicit path.

### Migrating from `gulpfile.js`

Move the task entry from JavaScript exports to one `gulp.mbtx` script at the
project root. A gulp task exported as `exports.clean = clean` becomes a branch
in the script dispatcher, and the default export becomes the fallback task when
no CLI task name is provided.

```js
exports.clean = clean;
exports.build = gulp.series(clean, scripts);
exports.default = exports.build;
```

```moonbit
///|
import {
  "moonbitlang/core/env" @env,
  "i5ting/gulp/entry" @entry,
}

///|
fn clean(_ctx : @core.Context) -> Result[Unit, @core.MulpError] { Ok(()) }
fn build(_ctx : @core.Context) -> Result[Unit, @core.MulpError] { Ok(()) }

///|
fn main {
  @entry.run_tasks([("clean", clean), ("build", build)], @env.args())
}
```

`@entry.run_tasks` handles `--tasks`, `--tree`, `--tasks-json`, and task dispatch
automatically — no hand-written argument dispatcher is needed. Replace
`gulpfile.js`, `gulpfile.mjs`, and transpiled gulpfiles with `gulp.mbtx`; the
CLI passes task arguments to `moon run --target native <gulp.mbtx> -- <args>`.
`gulp --tasks-simple` is accepted as a gulp-compatible alias and is normalized
to `--tasks` before invoking `gulp.mbtx`.

MoonBit stream helpers mirror gulp's `src().pipe(...).pipe(dest())` shape in
the library packages. Standalone `.mbtx` scripts import those helpers through a
registry-resolvable module path, for example
`"i5ting/gulp@0.1.0/entry"` or `"i5ting/gulp@0.1.0/stream"`.

For repository verification, `scripts/test_mbtx_entry_parallel.mjs` packages the
local module into an isolated registry cache, runs a real `gulp.mbtx`, and
checks that `@entry.parallel([clean, scripts])` completes near one sleep.

### Migration Checklist

1. Create one `gulp.mbtx` at the project root.
2. Move exported gulp tasks into named MoonBit functions and register them
   with `@entry.run_tasks([("name", fn), ...], @env.args())`.
3. Map `exports.default` to the `default_task` parameter of `run_tasks`.
4. Replace `gulp.series(...)` and `gulp.parallel(...)` with `@entry.series([...])`
   and `@entry.parallel([...])`.
5. Replace `gulp.src(globs).pipe(plugin()).pipe(gulp.dest(outDir))` with
   `@vinyl_fs.src(...).pipe_stream(@through2.object_transform_vinyl(...)).pipe(@vinyl_fs.dest(...))`
   for Vinyl pipelines, or `@stream.file_stream(...).pipe_stream(@through2.object_transform_files(...))`
   for lower-level file pipelines.
6. Replace Node callback tasks with `@core.async_task_from_callback(...)`.
7. Replace `gulp.watch(globs, task)` with `gulp --watch task` for CLI workflows
   or `@platform_async.async_watch_loop(...)` for embedded watch loops.
8. Run `gulp --tasks` and `gulp --tree build` after migration to verify the task
   registry output matches the old gulp CLI shape.

### Current Incompatibilities

`gulp` is intentionally MoonBit-native and does not execute JavaScript
gulpfiles. `gulpfile.js`, `gulpfile.mjs`, Babel/TypeScript-transpiled gulpfiles,
and npm gulp plugins must be migrated to `gulp.mbtx`, MoonBit transforms, or
external process tasks.

Node stream plugins are not loaded directly. Port the transform to MoonBit when
it is part of the file pipeline, or wrap tools such as TypeScript, Rollup, or
MoonBit builds as process tasks. The plugin examples package contains simple
rename/filter/concat transforms and process wrappers for TypeScript and MoonBit
build commands.

Native OS watch backends are still being filled in. The async watch loop already
handles debounce, pending rebuilds, cancellation, and failure recovery, while
the current native watcher uses a polling snapshot facade until macOS, Linux,
and Windows event backends are implemented.

### Common gulp Recipe Mappings

| gulp recipe | gulp shape |
| --- | --- |
| `gulp.src(globs).pipe(plugin()).pipe(gulp.dest(outDir))` | Use `@vinyl_fs.src(...)`, `@through2.object_transform_vinyl(...)`, and `@vinyl_fs.dest(...)`. |
| `gulp.series(clean, gulp.parallel(styles, scripts))` | Register `clean`, `styles`, and `scripts`, then expose `@entry.series([clean, @entry.parallel([styles, scripts])])`. |
| `gulp.watch(globs, task)` | Use `gulp --watch task` from the CLI, or wire `@platform_async.async_watch_loop(...)` when embedding a watcher. |
| `gulp.src(globs, { since: gulp.lastRun(task) })` | Use `@core.last_run(...)` / `@core.last_run_async(...)` with `src(..., since=...)`. |
| Callback task `function(done) { ...; done(err) }` | Use `@core.async_task_from_callback(...)`; duplicate completion is rejected. |
| Stream-returning task | Use `@stream_async.async_task_from_stream(...)` so the task completes when the stream drains or fails. |
| `gulp --tasks-simple` | Use `gulp --tasks-simple`; it is normalized to `--tasks` for `gulp.mbtx` scripts. |
| `gulp --tasks` / `gulp --tree` | Handled automatically by `@entry.run_tasks(...)` — no explicit dispatcher needed. |

### Gulp CLI Compatibility

| gulp CLI flag or behavior | gulp status | Notes |
| --- | --- | --- |
| `gulp` default task | Supported as `gulp` | The selected default is defined by the `gulp.mbtx` dispatcher. |
| `gulp <task>` | Supported as `gulp <task>` | One task name is accepted by the native CLI and forwarded to `gulp.mbtx`. |
| `gulp <task> <task>` concurrent CLI tasks | Not supported yet | Use an explicit `@entry.parallel([...])` task in `gulp.mbtx`. |
| `-v` / `--version` | Supported | Prints the native `gulp` version. |
| `--cwd <dir>` | Supported | Discovery and command execution use the supplied working directory. |
| `--gulpfile <path>` | MoonBit-compatible alias | Points to `gulp.mbtx` or explicit legacy `gulp.mbt`; it does not execute JS gulpfiles. |
| `--file <path>` / `--gulpfile <path>` | Supported | `--gulpfile` is the preferred native spelling. |
| `--tasks-simple` | Supported | Normalized to `--tasks` before script execution. |
| `--tasks` | Supported with gulp semantics | `gulp` expects the script to print the task list; gulp's decorated tree output is not cloned. |
| `-T` | Supported as an alias for `--tasks` | It lists tasks with current gulp semantics rather than gulp's decorated task tree. |
| `--require <module>` | Not supported | `gulp.mbtx` is MoonBit script mode, not a JS/transpiled gulpfile. |
| `--verify` | Not supported | npm plugin blacklist verification does not apply to native MoonBit plugins. |
| `--color` / `--no-color` / `--silent` | Not supported yet | Current output is plain stdout/stderr from the script and native command. |

### Task Completion Migration

gulp accepts several task completion shapes:

```js
exports.build = gulp.series(clean, gulp.parallel(styles, scripts));
exports.assets = function assets() {
  return src("src/**/*").pipe(dest("dist"));
};
exports.deploy = function deploy(done) {
  upload(function (err) {
    done(err);
  });
};
```

In `i5ting/gulp/core`, the native async equivalents are explicit task values. Use
`@entry.series([clean, build])` and `@entry.parallel([styles, scripts])` for
task-level ordering and concurrency, wrap callback-style work with
`@core.async_task_from_callback`, and wrap stream completion with
`@stream_async.async_task_from_stream`.

```moonbit
let clean = @entry.async_task(registry, "clean", fn(_ctx) -> Result[Unit, @core.MulpError] {
  Ok(())
})
let styles = @entry.async_task(registry, "styles", fn(_ctx) -> Result[Unit, @core.MulpError] {
  Ok(())
})
let scripts = @entry.async_task(registry, "scripts", fn(_ctx) -> Result[Unit, @core.MulpError] {
  Ok(())
})
ignore(@entry.async_task_from(
  registry,
  "build",
  @entry.series([clean, @entry.parallel([styles, scripts])]),
))
```

Stream-returning gulp tasks map to a task whose completion is the stream drain:

```moonbit
let assets = @stream_async.async_task_from_stream(
  "assets",
  @stream.src(files).pipe(@stream.dest("dist", sink)),
)
```

Callback-style gulp tasks map to a callback adapter that waits until the
callback fires and rejects duplicate completion:

```moonbit
let deploy = @core.async_task_from_callback("deploy", async fn(done) -> Unit {
  @async.sleep(10)
  done(Ok(()))
})
```

The completion validator rejects mixed modes such as a task that returns a
stream and also declares callback or process completion. This mirrors gulp's
rule that a task should use one completion signal.

### Node Plugin Compatibility

`gulp` does not load npm gulp plugins directly. gulp plugins are JavaScript
Transform streams that run inside Node, while `gulp` plugins are MoonBit
transforms over native `FileStream` and `ByteStream` values. Port plugin logic
to a MoonBit transform, wrap an external tool as a process task, or keep that
part of the pipeline in gulp until a dedicated adapter exists.

For plugin logic that was previously written with `through2`, prefer
`i5ting/gulp/through2`. It is not a JavaScript API clone: it exposes typed
MoonBit transforms (`transform_bytes`, `object_transform_files`,
`object_transform_vinyl`, lower-level `through_files` / `through_vinyl`, and
fresh-state `ctor_*` factories) instead of Node callback, `this.push`, and
`TransformOptions` objects.

Common `through2` callback patterns map to explicit `Result` values:

| JS through2 pattern | MoonBit through2 shape |
| --- | --- |
| `through2()` passthrough | `@through2.transform_bytes()` |
| `through2.obj(fn)` | `@through2.object_transform_files(map=Some(fn))` or `@through2.object_transform_vinyl(map=Some(fn))` |
| `through2.ctor(fn)` | `@through2.ctor_files(fn)` / `@through2.ctor_vinyl(fn)` / `@through2.ctor_bytes(fn)` |
| `cb(null, chunk)` | `Ok(Some(chunk))` |
| no output / filtered chunk | `Ok(None)` |
| `this.push(a); this.push(b); cb()` | `Ok([a, b])` via `*_flat` helpers |
| `_flush(cb)` | `flush=Some(fn() -> Result[Array[_], @core.MulpError] { ... })` |
| `cb(err)` / thrown error | `Err(@core.stream_error("message"))` |

### Gulp API Mapping

| gulp API | gulp API | Notes |
| --- | --- | --- |
| `src(globs, options)` | `@vinyl_fs.src(globs, @vinyl_fs.src_options(...))` or lower-level `@stream.src(files)` | `vinyl_fs` reads the native filesystem and preserves Vinyl-style `cwd`, `base`, `relative`, metadata, buffered, and streaming contents. `allow_empty`, `remove_bom`, `encoding=false`, and `sourcemaps=true` map to gulp's `allowEmpty`, `removeBOM`, raw byte, and adjacent `.map` loading behavior. |
| `dest(outDir, options)` | `@vinyl_fs.dest(outDir)` / `@vinyl_fs.dest_with_options(...)` or lower-level `@stream.file_dest(...)` | Writes real files and returns files with updated destination paths. `mode`, `dir_mode`, and `mtime_ms` metadata are applied to native filesystem entries; `overwrite=false`, `append=true`, external `sourcemaps=Some(".")`, and `inline_sourcemaps=true` cover the matching vinyl-fs write modes. |
| `symlink(outDir, options)` | `@vinyl_fs.symlink(outDir)` / `@vinyl_fs.symlink_with_options(...)` | Emits filesystem symlinks where the native platform supports them; `relative_symlinks=true` maps to vinyl-fs `relativeSymlinks`, and `overwrite=false` preserves an existing destination link. |
| `new Vinyl({ sourceMap })` | `@vinyl.vinyl_file(source_map=...)` | MoonBit keeps custom JS-style props in typed fields or metadata; `source_map` is first class. |
| `task(name, fn)` | `@entry.run_tasks([("name", fn), ...], args)` — or `@entry.task(registry, ...)` when task handles are needed for `series`/`parallel`. | `run_tasks` is the primary pattern for `gulp.mbtx`. |
| `series(...)` | `@entry.series([...])` / `@core.series([...])` | Preserves left-to-right task order. |
| `parallel(...)` | `@entry.parallel([...])` / `@core.async_parallel([...])` | Native async tasks run concurrently with shared cancellation. |
| `watch(globs, task)` | `@platform_async.async_watch_loop(...)` plus CLI watch dispatch | Event normalization and pending rebuild behavior are implemented; native OS event backends are still a TODO. |
| `lastRun(task)` | `@core.last_run(...)` / `@core.last_run_async(...)` | Integrates with `src(..., since=...)` style filtering. |
| callback completion | `@core.async_task_from_callback(...)` | Duplicate completion and mixed completion modes are rejected. |
| stream completion | `@stream_async.async_task_from_stream(...)` | A stream task completes when the stream drains or fails. |

### Migrating from `gulp.toml`

`gulp.toml` was an entry metadata file that pointed at a separate MoonBit entry
package. `gulp.mbtx` replaces that model with a single script-mode entry file.

Use this mapping:

```text
gulp.toml entry package -> gulp.mbtx script
gulp.toml task metadata -> --tasks / --tree output from gulp.mbtx
moon run <entry> -- <task> -> moon run --target native <gulp.mbtx> -- <task>
```

Bare discovery now searches upward only for `gulp.mbtx`. It does not
automatically fall back to `gulp.toml` or legacy `gulp.mbt` metadata. Existing
compatibility tests can still pass an explicit path with `--config`,
`--gulpfile`, or `--file`, but new projects should create `gulp.mbtx` and put
their task dispatch there.

### Legacy Task Entry Convention

The older explicit `gulp.mbt` compatibility path still supports a MoonBit build
entry package. That package registers tasks by exposing a function that
matches `@entry.RegisterTasks`. By convention, name it `register_tasks`; it
receives a `Registry` owned by the entry runtime and adds task definitions to it:

```moonbit
pub fn register_tasks(registry : @core.Registry) -> Unit {
  ignore(@entry.task(registry, "build", fn(_ctx) -> Result[Unit, @core.MulpError] {
    Ok(())
  }))
}
```

Later entry helpers will create the registry, call this function, and use the
registered tasks for CLI execution and task list/tree output.

For tests or custom entry wiring, create the registry through the entry helper:

```moonbit
let registry = @entry.new_registry()
register_tasks(registry)
```

An entry `main` can pass `@env.args()` to `run_args`; the helper runs the task
name supplied by `moon run <entry> -- <task>`:

```moonbit
fn main {
  let registry = @entry.new_registry()
  register_tasks(registry)
  let ctx = @core.new_context(
    cwd=".",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  @entry.run_args(registry, @env.args(), ctx).unwrap()
}
```

Use `render_args` for entry-local task discovery output:

```moonbit
let output = @entry.render_args(registry, ["build", "--tasks"]).unwrap()
```

Async entry wiring uses the async registry and async run helper:

```moonbit
async fn main {
  let registry = @entry.new_async_registry()
  let clean = @entry.async_task(registry, "clean", fn(_ctx) -> Result[Unit, @core.MulpError] {
    Ok(())
  })
  let scripts = @entry.async_task(registry, "scripts", fn(_ctx) -> Result[Unit, @core.MulpError] {
    Ok(())
  })
  ignore(@entry.async_task_from(registry, "build", @core.async_parallel([clean, scripts])))
  let ctx = @core.new_context(
    cwd=".",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  @entry.async_run_entry_args(registry, @env.args(), ctx, fn(output) {
    println(output)
  }).unwrap()
}
```

See `examples/basic` for a minimal `gulp.mbtx` task script.

The CLI accepts `gulp --watch build` and proxies it to
`moon run --target native <gulp.mbtx> -- --watch build`; the current script runs
the named task once. The platform async watch loop already supports debounce,
mid-run pending rebuilds, and rerun after completion, but the CLI still needs to
wire that loop to a long-running native filesystem watcher.

At the platform layer, `watch_glob(paths, pattern)` keeps a matched-path
snapshot and `poll(next_paths)` reports added/removed matching paths.
`debouncer(delay_ms)` can then coalesce repeated watch events until the quiet
window has elapsed.
`watch_run_state()` tracks whether a task is currently running and records a
pending rebuild when file changes arrive mid-run.
Task failures are recorded on the watch state without deactivating the watcher,
so later changes can still trigger another run.
Stopping the watch state cancels the active token, clears pending rebuilds, and
marks the watcher inactive.
