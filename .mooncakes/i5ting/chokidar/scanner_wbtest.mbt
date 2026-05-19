///|
test "snapshot diff emits add change and unlink events" {
  let old_entries = [
    SnapshotEntry::{
      path: "a.txt",
      kind: File,
      stats: Some({ size: 1L, mode: 0, modified_nanos: 1L }),
    },
    SnapshotEntry::{
      path: "gone.txt",
      kind: File,
      stats: Some({ size: 1L, mode: 0, modified_nanos: 1L }),
    },
  ]
  let new_entries = [
    SnapshotEntry::{
      path: "a.txt",
      kind: File,
      stats: Some({ size: 2L, mode: 0, modified_nanos: 2L }),
    },
    SnapshotEntry::{
      path: "new.txt",
      kind: File,
      stats: Some({ size: 1L, mode: 0, modified_nanos: 1L }),
    },
  ]
  let events = diff_snapshots(old_entries, new_entries)
  let names = events.map(event => event.name)
  inspect(names, content="[Change, Add, Unlink]")
}

///|
test "snapshot diff replaces file with directory at same path" {
  let old_entries = [
    SnapshotEntry::{
      path: "node",
      kind: File,
      stats: Some({ size: 1L, mode: 0, modified_nanos: 1L }),
    },
  ]
  let new_entries = [
    SnapshotEntry::{
      path: "node",
      kind: Directory,
      stats: Some({ size: 0L, mode: 0, modified_nanos: 2L }),
    },
  ]
  let events = diff_snapshots(old_entries, new_entries)
  inspect(events.map(event => event.name), content="[Unlink, AddDir]")
  inspect(events.map(event => event.path), content="[\"node\", \"node\"]")
  assert_true(events[0].stats is None)
}

///|
test "snapshot diff replaces directory with file at same path" {
  let old_entries = [
    SnapshotEntry::{
      path: "node",
      kind: Directory,
      stats: Some({ size: 0L, mode: 0, modified_nanos: 1L }),
    },
  ]
  let new_entries = [
    SnapshotEntry::{
      path: "node",
      kind: File,
      stats: Some({ size: 1L, mode: 0, modified_nanos: 2L }),
    },
  ]
  let events = diff_snapshots(old_entries, new_entries)
  inspect(events.map(event => event.name), content="[UnlinkDir, Add]")
  inspect(events.map(event => event.path), content="[\"node\", \"node\"]")
  assert_true(events[0].stats is None)
}

///|
test "snapshot diff orders output deterministically" {
  let old_entries = [
    SnapshotEntry::{
      path: "z.txt",
      kind: File,
      stats: Some({ size: 1L, mode: 0, modified_nanos: 1L }),
    },
    SnapshotEntry::{
      path: "a.txt",
      kind: File,
      stats: Some({ size: 1L, mode: 0, modified_nanos: 1L }),
    },
  ]
  let new_entries = [
    SnapshotEntry::{
      path: "m.txt",
      kind: File,
      stats: Some({ size: 1L, mode: 0, modified_nanos: 1L }),
    },
    SnapshotEntry::{
      path: "a.txt",
      kind: File,
      stats: Some({ size: 2L, mode: 0, modified_nanos: 2L }),
    },
  ]
  let events = diff_snapshots(old_entries, new_entries)
  inspect(events.map(event => event.name), content="[Change, Add, Unlink]")
  inspect(
    events.map(event => event.path),
    content="[\"a.txt\", \"m.txt\", \"z.txt\"]",
  )
}

///|
test "scanner respects ignored and depth options" {
  let root = "./_build/readdirp-chokidar-scan"
  test_remove_tree(root)
  test_create_dir(root)
  test_create_dir(join_path(root, "sub"))
  test_write_file(join_path(root, "keep.txt"), "keep")
  test_write_file(join_path(root, "skip.tmp"), "skip")
  test_write_file(join_path(root, "sub/deep.txt"), "deep")
  let options = Options::{
    ..Options::default(),
    ignored: [MatchFunction((path, _stats) => path.has_suffix(".tmp"))],
    depth: 0,
    always_stat: true,
  }
  let snapshot = scan_snapshot(root, options)
  let paths = snapshot.map(entry => entry.path)
  inspect(paths, content="[\"keep.txt\", \"sub\"]")
  test_remove_tree(root)
}

///|
test "scanner test cleanup removes stale files" {
  let root = "./_build/readdirp-chokidar-scan-cleanup"
  test_remove_tree(root)
  test_create_dir(root)
  test_write_file(join_path(root, "stale.txt"), "stale")
  test_remove_tree(root)
  test_create_dir(root)
  test_write_file(join_path(root, "keep.txt"), "keep")
  let snapshot = scan_snapshot(root, Options::default())
  let paths = snapshot.map(entry => entry.path)
  inspect(paths, content="[\"keep.txt\"]")
  test_remove_tree(root)
}

///|
test "scanner detects same size rewrite from modified timestamp" {
  let root = "./_build/readdirp-chokidar-scan-same-size"
  test_remove_tree(root)
  test_create_dir(root)
  let path = join_path(root, "same.txt")
  test_write_file(path, "aaaa")
  let old_entries = scan_snapshot(root, Options::default())
  test_sleep_millis(20)
  test_write_file(path, "bbbb")
  let new_entries = scan_snapshot(root, Options::default())
  let events = diff_snapshots(old_entries, new_entries)
  inspect(events.map(event => event.name), content="[Change]")
  test_remove_tree(root)
}

///|
test "scanner handles symlink loop without infinite recursion" {
  let root = "./_build/readdirp-chokidar-scan-symloop"
  test_remove_tree(root)
  test_create_dir(root)
  test_create_dir(join_path(root, "dir_a"))
  test_create_dir(join_path(root, "dir_a/dir_b"))
  test_write_file(join_path(root, "dir_a/file.txt"), "content")
  test_create_symlink("../../dir_a", join_path(root, "dir_a/dir_b/loop_back"))
  let options = Options::{ ..Options::default(), follow_symlinks: true }
  let snapshot = scan_snapshot(root, options)
  let paths = snapshot.map(entry => entry.path)
  assert_false(paths.contains("dir_a/dir_b/loop_back/dir_b/loop_back"))
  test_remove_tree(root)
}

///|
test "scanner warning handling respects ignore permission errors" {
  let warnings = [
    @readdirp.ReaddirpWarning::{
      path: "private",
      kind: @readdirp.PermissionDenied,
      message: "permission denied",
    },
  ]
  let ignored = Options::{
    ..Options::default(),
    ignore_permission_errors: true,
  }
  assert_true(warnings_to_error(warnings, ignored) is None)
  let strict = Options::{
    ..Options::default(),
    ignore_permission_errors: false,
  }
  let message = try {
    match warnings_to_error(warnings, strict) {
      Some(error) => raise error
      None => ()
    }
    ""
  } catch {
    ChokidarError(message) => message
  }
  inspect(message, content="scan warning for private: permission denied")
}

///|
test "scanner depth=0 returns only root-level entries" {
  let root = "./_build/readdirp-chokidar-scan-depth0"
  test_remove_tree(root)
  test_create_dir(root)
  test_create_dir(join_path(root, "sub"))
  test_write_file(join_path(root, "a.txt"), "a")
  test_write_file(join_path(root, "sub/b.txt"), "b")
  let snapshot = scan_snapshot(root, Options::{ ..Options::default(), depth: 0 })
  let paths = snapshot.map(entry => entry.path)
  inspect(paths, content="[\"a.txt\", \"sub\"]")
  test_remove_tree(root)
}

///|
test "scanner depth=1 returns entries one level deep" {
  let root = "./_build/readdirp-chokidar-scan-depth1"
  test_remove_tree(root)
  test_create_dir(root)
  test_create_dir(join_path(root, "sub"))
  test_create_dir(join_path(root, "sub/deep"))
  test_write_file(join_path(root, "a.txt"), "a")
  test_write_file(join_path(root, "sub/b.txt"), "b")
  test_write_file(join_path(root, "sub/deep/c.txt"), "c")
  let snapshot = scan_snapshot(root, Options::{ ..Options::default(), depth: 1 })
  let paths = snapshot.map(entry => entry.path)
  inspect(
    paths,
    content="[\"a.txt\", \"sub\", \"sub/b.txt\", \"sub/deep\"]",
  )
  test_remove_tree(root)
}

///|
test "scanner depth=2 returns entries two levels deep" {
  let root = "./_build/readdirp-chokidar-scan-depth2"
  test_remove_tree(root)
  test_create_dir(root)
  test_create_dir(join_path(root, "sub"))
  test_create_dir(join_path(root, "sub/deep"))
  test_create_dir(join_path(root, "sub/deep/deeper"))
  test_write_file(join_path(root, "a.txt"), "a")
  test_write_file(join_path(root, "sub/b.txt"), "b")
  test_write_file(join_path(root, "sub/deep/c.txt"), "c")
  test_write_file(join_path(root, "sub/deep/deeper/d.txt"), "d")
  let snapshot = scan_snapshot(root, Options::{ ..Options::default(), depth: 2 })
  let paths = snapshot.map(entry => entry.path)
  inspect(
    paths,
    content=
      "[\"a.txt\", \"sub\", \"sub/b.txt\", \"sub/deep\", \"sub/deep/c.txt\", \"sub/deep/deeper\"]",
  )
  test_remove_tree(root)
}

///|
test "scanner respects MatchRegex ignored pattern" {
  let root = "./_build/readdirp-chokidar-scan-regex"
  test_remove_tree(root)
  test_create_dir(root)
  test_write_file(join_path(root, "keep.txt"), "keep")
  test_write_file(join_path(root, "skip.log"), "skip")
  test_write_file(join_path(root, "also.log"), "also")
  let re = @regexp.compile("\\.log$") catch { _ => panic() }
  let options = Options::{
    ..Options::default(),
    ignored: [MatchRegex(re)],
  }
  let snapshot = scan_snapshot(root, options)
  let paths = snapshot.map(entry => entry.path)
  inspect(paths, content="[\"keep.txt\"]")
  test_remove_tree(root)
}

///|
test "scanner follows symlinked dirs when follow_symlinks=true" {
  let root = "./_build/readdirp-chokidar-scan-symlink-follow"
  let real = "./_build/readdirp-chokidar-scan-symlink-real"
  test_remove_tree(root)
  test_remove_tree(real)
  test_create_dir(root)
  test_create_dir(real)
  test_write_file(join_path(real, "inside.txt"), "content")
  test_create_symlink("../" + path_basename(real), join_path(root, "link"))
  let options = Options::{ ..Options::default(), follow_symlinks: true }
  let snapshot = scan_snapshot(root, options)
  let paths = snapshot.map(entry => entry.path)
  assert_true(paths.contains("link"))
  assert_true(paths.contains("link/inside.txt"))
  test_remove_tree(root)
  test_remove_tree(real)
}

///|
test "scanner with follow_symlinks traverses into symlinked directory" {
  let root = "./_build/readdirp-chokidar-scan-symlink-nofollow"
  let real = "./_build/readdirp-chokidar-scan-symlink-nofollow-real"
  test_remove_tree(root)
  test_remove_tree(real)
  test_create_dir(root)
  test_create_dir(real)
  test_write_file(join_path(real, "inside.txt"), "content")
  test_create_symlink("../" + path_basename(real), join_path(root, "link"))
  let options = Options::{ ..Options::default(), follow_symlinks: true }
  let snapshot = scan_snapshot(root, options)
  let paths = snapshot.map(entry => entry.path)
  assert_true(paths.contains("link"))
  assert_true(paths.contains("link/inside.txt"))
  test_remove_tree(root)
  test_remove_tree(real)
}

///|
test "scanner ignored MatchPath uses full path relative to event form" {
  let root = "./_build/readdirp-chokidar-scan-match"
  test_remove_tree(root)
  test_create_dir(root)
  test_write_file(join_path(root, "keep.txt"), "keep")
  test_write_file(join_path(root, "skip.txt"), "skip")
  let options = Options::{
    ..Options::default(),
    ignored: [MatchPath(join_path(root, "skip.txt"))],
  }
  let paths = scan_snapshot(root, options).map(entry => entry.path)
  assert_true(paths.contains("keep.txt"))
  assert_false(paths.contains("skip.txt"))
  test_remove_tree(root)
}

///|
test "scanner filters unreadable files when ignore_permission_errors=false" {
  let root = "./_build/readdirp-chokidar-scan-perms"
  test_remove_tree(root)
  test_create_dir(root)
  test_write_file(join_path(root, "readable.txt"), "readable")
  test_write_file(join_path(root, "noperm.txt"), "private")
  test_chmod(join_path(root, "noperm.txt"), 0x00)
  let strict_options = Options::{
    ..Options::default(),
    ignore_permission_errors: false,
  }
  let strict_paths = scan_snapshot(root, strict_options).map(entry => entry.path)
  assert_false(strict_paths.contains("noperm.txt"))
  assert_true(strict_paths.contains("readable.txt"))
  let lenient_options = Options::{
    ..Options::default(),
    ignore_permission_errors: true,
  }
  let lenient_paths = scan_snapshot(root, lenient_options).map(
    entry => entry.path,
  )
  assert_true(lenient_paths.contains("noperm.txt"))
  test_chmod(join_path(root, "noperm.txt"), 0o644)
  test_remove_tree(root)
}
