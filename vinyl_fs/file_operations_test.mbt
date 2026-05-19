///|
test "vinyl-fs file operations read file metadata" {
  let root = "/tmp/mulp-vinyl-fs-file-operations"
  write_text_fixture(root, "meta.txt", "hello")
  let metadata = file_metadata(root + "/meta.txt").unwrap()
  inspect(metadata.kind(), content="file")
  inspect(metadata.size().unwrap(), content="5")
  inspect(metadata.mode() is Some(_), content="true")
  inspect(metadata.mtime_ms() is Some(_), content="true")
}

///|
test "vinyl-fs file operations report missing metadata as errors" {
  let result = file_metadata("/tmp/mulp-vinyl-fs-file-operations/missing.txt")
  inspect(result is Err(_), content="true")
}

///|
test "vinyl-fs file operations read symlink targets" {
  if src_symlink_supported_for_test() {
    let root = "/tmp/mulp-vinyl-fs-file-operations-symlink"
    write_text_fixture(root, "target.txt", "target")
    write_symlink_fixture(root, "link.txt", root + "/target.txt")
    inspect(read_symlink_target_file(root + "/link.txt").unwrap(), content=root + "/target.txt")
  } else {
    inspect(src_symlink_supported_for_test(), content="false")
  }
}
