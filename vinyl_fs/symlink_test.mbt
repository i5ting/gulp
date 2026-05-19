///|
fn symlink_test_context() -> @core.Context {
  @core.new_context(
    cwd="/tmp",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
}

///|
test "symlink writes links to input file paths" {
  if symlink_supported_for_test() {
    write_text_fixture(
      "/tmp/mulp-vinyl-fs-symlink-source",
      "target.txt",
      "target",
    )
    let input = @vinyl.vinyl_file(
      cwd="/tmp/mulp-vinyl-fs-symlink-source",
      base="/tmp/mulp-vinyl-fs-symlink-source",
      path=Some("/tmp/mulp-vinyl-fs-symlink-source/target.txt"),
      contents=@vinyl.Null,
    )
    let outputs = vinyl_file_stream([input])
      .pipe(symlink("/tmp/mulp-vinyl-fs-symlink-out"))
      .collect(symlink_test_context())
      .unwrap()
    inspect(
      outputs[0].path().unwrap(),
      content="/tmp/mulp-vinyl-fs-symlink-out/target.txt",
    )
    inspect(outputs[0].is_symbolic(), content="true")
    let linked = @stream.file_byte_stream(
      "/tmp/mulp-vinyl-fs-symlink-out/target.txt",
      chunk_size=16,
    ).unwrap()
    inspect(linked.read_all(max_bytes=16).unwrap(), content="target")
  } else {
    inspect(symlink_supported_for_test(), content="false")
  }
}

///|
test "symlink emits symbolic vinyl objects with preserved target" {
  if symlink_supported_for_test() {
    let input = @vinyl.vinyl_file(
      cwd="/workspace",
      base="/workspace/src",
      path=Some("/workspace/src/target.txt"),
      contents=@vinyl.Buffer(@utf8.encode("target")),
    )
    let outputs = vinyl_file_stream([input])
      .pipe(symlink("/tmp/mulp-vinyl-fs-symlink-symbolic"))
      .collect(symlink_test_context())
      .unwrap()
    inspect(outputs.length(), content="1")
    inspect(outputs[0].is_symbolic(), content="true")
    inspect(outputs[0].symlink().unwrap(), content="/workspace/src/target.txt")
  } else {
    inspect(symlink_supported_for_test(), content="false")
  }
}

///|
test "symlink can write relative symlink targets" {
  if symlink_supported_for_test() {
    let source = "/tmp/mulp-vinyl-fs-relative-symlink-source"
    let out = "/tmp/mulp-vinyl-fs-relative-symlink-out"
    write_text_fixture(source, "target.txt", "target")
    let input = @vinyl.vinyl_file(
      cwd=source,
      base=source,
      path=Some(source + "/target.txt"),
      contents=@vinyl.Null,
    )
    let outputs = vinyl_file_stream([input])
      .pipe(symlink_with_options(out, dest_options(relative_symlinks=true)))
      .collect(symlink_test_context())
      .unwrap()
    inspect(outputs[0].symlink().unwrap(), content="../mulp-vinyl-fs-relative-symlink-source/target.txt")
    inspect(read_symlink_target_file(out + "/target.txt").unwrap(), content="../mulp-vinyl-fs-relative-symlink-source/target.txt")
  } else {
    inspect(symlink_supported_for_test(), content="false")
  }
}

///|
extern "c" fn symlink_supported_for_test() -> Bool = "mulp_symlink_supported_for_test"
