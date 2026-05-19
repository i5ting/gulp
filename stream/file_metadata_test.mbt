///|
fn metadata_value(metadata : Array[(String, String)], key : String) -> String? {
  for item in metadata {
    if item.0 == key {
      return Some(item.1)
    }
  }
  None
}

///|
test "real file metadata includes kind size mtime and mode" {
  let metadata = real_file_metadata("stream/fixtures/chunked.txt").unwrap()
  inspect(metadata_value(metadata, "kind").unwrap(), content="file")
  inspect(metadata_value(metadata, "size").unwrap(), content="7")
  inspect(metadata_value(metadata, "mtime_ms").unwrap() != "", content="true")
  inspect(metadata_value(metadata, "mode").unwrap() != "", content="true")
}

///|
test "real file metadata rejects missing paths" {
  let result = real_file_metadata("stream/fixtures/missing.txt")
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="File system error: cannot stat path: stream/fixtures/missing.txt",
  )
}
