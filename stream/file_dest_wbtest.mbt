///|
test "file dest creates directories and symlinks" {
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let target = file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/mulp-symlink-target.txt",
    contents=Bytes(byte_stream(["target"])),
  )
  ignore(src([target]).pipe(file_dest("/tmp")).collect(ctx))
  let directory = file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/mulp-dir-output",
    contents=Directory,
  )
  let symlink = file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/mulp-link-output.txt",
    contents=Symlink("/tmp/mulp-symlink-target.txt"),
  )
  ignore(src([directory, symlink]).pipe(file_dest("/tmp")).collect(ctx))
  inspect(path_is_directory_for_test("/tmp/mulp-dir-output"), content="true")
  if symlink_supported_for_test() {
    inspect(path_is_symlink_for_test("/tmp/mulp-link-output.txt"), content="true")
    let linked = file_byte_stream("/tmp/mulp-link-output.txt", chunk_size=8).unwrap()
    inspect(linked.read_all(max_bytes=8).unwrap(), content="target")
  } else {
    inspect(path_is_symlink_for_test("/tmp/mulp-link-output.txt"), content="false")
  }
}

///|
#borrow(path)
extern "c" fn path_is_directory_for_test_ffi(path : Bytes) -> Bool = "mulp_path_is_directory_for_test"

///|
#borrow(path)
extern "c" fn path_is_symlink_for_test_ffi(path : Bytes) -> Bool = "mulp_path_is_symlink_for_test"

///|
extern "c" fn symlink_supported_for_test() -> Bool = "mulp_symlink_supported_for_test"

///|
fn path_is_directory_for_test(path : String) -> Bool {
  path_is_directory_for_test_ffi(@utf8.encode(path))
}

///|
fn path_is_symlink_for_test(path : String) -> Bool {
  path_is_symlink_for_test_ffi(@utf8.encode(path))
}
