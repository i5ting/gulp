///|
test "vinyl-fs sourcemap attaches source maps to vinyl files" {
  let file = @vinyl.vinyl_file(
    cwd="/workspace",
    base="/workspace/src",
    path=Some("/workspace/src/app.js"),
    contents=@vinyl.Buffer(@utf8.encode("console.log(1)")),
  )
  let mapped = attach_source_map(file, "{\"version\":3}")
  inspect(mapped.source_map().unwrap(), content="{\"version\":3}")
  inspect(mapped.path().unwrap(), content="/workspace/src/app.js")
}

///|
test "vinyl-fs sourcemap creates external map vinyl files" {
  let file = @vinyl.vinyl_file(
    cwd="/workspace",
    base="/workspace/dist",
    path=Some("/workspace/dist/app.js"),
    source_map=Some("{\"version\":3}"),
  )
  let map = source_map_file(file, location=".").unwrap()
  inspect(map.path().unwrap(), content="/workspace/dist/app.js.map")
  inspect(map.relative(), content="app.js.map")
  match map.contents() {
    @vinyl.Buffer(bytes) => inspect(@utf8.decode_lossy(bytes), content="{\"version\":3}")
    _ => inspect("wrong contents", content="buffer")
  }
}

///|
test "vinyl-fs sourcemap skips files without source maps" {
  inspect(source_map_file(@vinyl.vinyl_file(), location=".") is None, content="true")
}
