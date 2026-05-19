# CLAUDE.md

This file provides guidance to Claude Code when working with code in this repository.

## Project Overview

`i5ting/gulp` is a MoonBit-native build tool inspired by the Node.js gulp task runner.  
Module name: `i5ting/gulp` | Build system: `moon`

## Requirements

1. 每个改动需要有测试用例覆盖
2. 确保测试和 benchmark 是通过的，固定 CPU/磁盘负载并多轮取 median，继续 benchmark
3. 遇到问题时，增加足够的 debug log，尽量不要猜测问题
4. 每个 task 完成后 commit 一次，补充测试用例，确保测试通过，如果运行有错误请修正
5. 确保与 gulp 功能对齐，如果 TODO 完成，请重新生成 TODO 到 goal.md，继续执行，直到完全对齐
6. 每个 task 执行过程中，如果执行失败，请进行下一个 task
7. 每个 task 执行失败，分析原因，重新生成 TODO 到 goal.md，继续执行，直到完全对齐
8. 实现 gulp 的真并发
9. 核心是 gulp.mbtx，采用脚本执行
10. 严格实现 gulp/vinyl，保证测试 100%
11. 严格实现 gulp/vinyl_fs，保证测试 100%

## Development Commands

### Build

```bash
moon build                                        # build all packages (wasm-gc default)
moon build --target native                        # build for native backend
moon build cmd/mulp --release --target native     # build the CLI binary (release)
```

### Test

```bash
moon test                                         # run all tests
moon test --target native                         # run native-only tests
moon test --package core                          # test a single package
moon test --package stream --target native
moon bench --package stream --target native
moon bench --package vinyl --target native
moon bench --package vinyl_fs --target native
```

### Run the CLI (development)

```bash
moon run cmd/mulp --target native -- --cwd examples/basic build
moon run cmd/mulp --target native -- --tasks
moon run cmd/mulp --target native -- --tree build
```

### Scripts (Node.js)

```bash
node scripts/release_smoke.mjs                    # end-to-end release smoke test
node scripts/bench_compare.mjs                    # compare native vs JS benchmark
node scripts/test_mbtx_entry_parallel.mjs         # test gulp.mbtx parallel entry
node scripts/check_docs.mjs                       # verify README doc consistency
```

### Install release binary

```bash
moon build cmd/mulp --release --target native
mkdir -p ~/.local/bin
cp _build/native/release/build/cmd/mulp/mulp.exe ~/.local/bin/gulp
chmod +x ~/.local/bin/gulp
```

## Package Structure

All packages live at the root level. Dependencies are declared in each package's `moon.pkg.json`.

| Package | Description |
| --- | --- |
| `core` | Task registration, `series`, `parallel`, `lastRun`, registry, cancellation, context |
| `stream` | `ByteStream`, `FileStream`, in-memory `src`/`dest`, native file I/O |
| `vinyl` | Vinyl file model (path history, stat, symlink, metadata, clone) |
| `vinyl_fs` | Filesystem `src`/`dest`/`symlink` with glob expansion and Vinyl semantics |
| `through2` | Typed transforms: `transform_bytes`, `object_transform_files`, `object_transform_vinyl` |
| `stream_pipeline` | Pipeline composition helpers |
| `stream_async` | Async stream task adapter and bounded worker pool |
| `cli` | Pure CLI argument parsing and task list/tree rendering |
| `cli_runtime` | CLI runtime helpers |
| `entry` | Task entry convention for `gulp.mbtx` scripts |
| `platform` | In-memory filesystem, process execution, glob snapshot, signals |
| `platform_async` | Async process tasks, `async_watch_loop` |
| `undertaker` | Task graph with `lastRun` tracking |
| `log_events` | Structured log event types |
| `cmd/mulp` | Native executable entry — discovers `gulp.mbtx`, dispatches tasks |
| `cmd/bench_compare` | Benchmark comparison helper |
| `plugin_examples` | Example transforms and process wrappers |
| `examples/basic` | Minimal `gulp.mbtx` task script |

## Dependencies

Declared in `moon.mod.json`:

- `moonbitlang/async` `0.19.0` — async/await runtime
- `i5ting/glob-watcher` `0.1.0` — glob-based file watcher
- `i5ting/chokidar` `0.1.0` — filesystem watcher backend
- `i5ting/readdirp` `0.1.3` — recursive directory reader

## Key Conventions

- **Native-only files**: Declared in `moon.pkg.json` under `"targets": { "file.mbt": ["native"] }`. Most I/O, process, and async code is native-only.
- **C stubs**: Native FFI shims listed under `"native-stub"` in `moon.pkg.json`.
- **Task entry**: User build scripts are `gulp.mbtx` files (MoonBit script mode). The CLI discovers them upward from CWD.
- **No `gulp.toml`/`gulp.mbt` auto-discovery**: Bare `gulp` searches for `gulp.mbtx` only; legacy paths require `--config`/`--file`/`--gulpfile`.
- **Test targets**: Tests for native-only code are also declared under `"targets"` in `moon.pkg.json`.

## Build Output

```
_build/native/release/build/cmd/mulp/mulp.exe   # release binary
_build/native/debug/test/                        # test build artifacts
```

