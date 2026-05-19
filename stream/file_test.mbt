///|
test "file stores path fields and byte contents" {
  let file = file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/app/main.mbt",
    contents=Bytes(byte_stream(["main"])),
  )
  inspect(file.cwd, content="/workspace")
  inspect(file.base, content="/workspace/src")
  inspect(file.path, content="/workspace/src/app/main.mbt")
  inspect(file.relative_path(), content="app/main.mbt")
}

///|
test "relative path falls back to full path outside base" {
  let file = file(
    cwd="/workspace",
    base="/workspace/src",
    path="/tmp/outside.mbt",
    contents=Empty,
  )
  inspect(file.relative_path(), content="/tmp/outside.mbt")
}

///|
test "relative path falls back for sibling prefix paths" {
  let file = file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src-other/a.mbt",
    contents=Empty,
  )
  inspect(file.relative_path(), content="/workspace/src-other/a.mbt")
}

///|
test "file can carry metadata pairs" {
  let file = file_with_metadata(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/app/main.mbt",
    contents=Empty,
    metadata=[("kind", "source")],
  )
  inspect(file.metadata.length(), content="1")
}

///|
test "file derives dirname basename stem and extname" {
  let file = file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/app/main.test.mbt",
    contents=Empty,
  )
  inspect(file.dirname(), content="/workspace/src/app")
  inspect(file.basename(), content="main.test.mbt")
  inspect(file.stem(), content="main.test")
  inspect(file.extname(), content=".mbt")
}

///|
test "file path parts handle names without extension" {
  let file = file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/Makefile",
    contents=Empty,
  )
  inspect(file.dirname(), content="/workspace/src")
  inspect(file.basename(), content="Makefile")
  inspect(file.stem(), content="Makefile")
  inspect(file.extname(), content="")
}

///|
test "file copy preserves fields and metadata" {
  let original = file_with_metadata(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/app.mbt",
    contents=Text("main"),
    metadata=[("mtime", "1000")],
  )
  let copied = original.copy()
  inspect(copied.cwd, content="/workspace")
  inspect(copied.base, content="/workspace/src")
  inspect(copied.path, content="/workspace/src/app.mbt")
  inspect(copied.metadata, content="[(\"mtime\", \"1000\")]")
  match copied.contents {
    Text(contents) => inspect(contents, content="main")
    _ => inspect("wrong contents", content="main")
  }
}

///|
test "file can carry source map and copy preserves it" {
  let original = file_with_source_map(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/app.js",
    contents=Text("console.log(1);"),
    source_map=Some("{\"version\":3}"),
  )
  let copied = original.copy()
  inspect(copied.source_map().unwrap(), content="{\"version\":3}")
}

///|
test "file with path recalculates relative path" {
  let original = file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/app/main.mbt",
    contents=Empty,
  )
  let renamed = original.with_path("/workspace/src/lib/index.mbt")
  inspect(renamed.path, content="/workspace/src/lib/index.mbt")
  inspect(renamed.relative_path(), content="lib/index.mbt")
  inspect(renamed.base, content="/workspace/src")
}

///|
test "file exposes stat mode mtime and symlink metadata" {
  let file = file_with_metadata(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/link",
    contents=Symlink("/workspace/target.txt"),
    metadata=[
      ("kind", "symlink"),
      ("mode", "120755"),
      ("mtime_ms", "1700000000000"),
    ],
  )
  inspect(file.kind().unwrap(), content="symlink")
  inspect(file.mode().unwrap(), content="120755")
  inspect(file.mtime_ms().unwrap(), content="1700000000000")
  inspect(file.symlink_target().unwrap(), content="/workspace/target.txt")
}
