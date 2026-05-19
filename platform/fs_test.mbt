///|
test "memory fs creates directories and writes files" {
  let fs = memory_file_system()
  fs.mkdir("/workspace").unwrap()
  fs.write_file("/workspace/app.txt", "hello").unwrap()
  inspect(fs.read_file("/workspace/app.txt").unwrap(), content="hello")
}

///|
test "memory fs stats files and directories" {
  let fs = memory_file_system()
  fs.mkdir("/workspace").unwrap()
  fs.write_file("/workspace/app.txt", "hello").unwrap()
  let dir = fs.stat("/workspace").unwrap()
  let file = fs.stat("/workspace/app.txt").unwrap()
  inspect(dir.kind is Directory, content="true")
  inspect(file.kind is RegularFile, content="true")
  inspect(file.size, content="5")
}

///|
test "memory fs deletes files and empty directories" {
  let fs = memory_file_system()
  fs.mkdir("/workspace").unwrap()
  fs.write_file("/workspace/app.txt", "hello").unwrap()
  fs.delete("/workspace/app.txt").unwrap()
  fs.delete("/workspace").unwrap()
  let result = fs.stat("/workspace")
  inspect(result is Err(_), content="true")
  inspect(result.unwrap_err().message(), content="File system error: no such path: /workspace")
}

///|
test "memory fs rejects writes when parent directory is missing" {
  let fs = memory_file_system()
  let result = fs.write_file("/workspace/app.txt", "hello")
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="File system error: missing parent directory: /workspace",
  )
}

///|
test "memory fs rejects deleting non-empty directories" {
  let fs = memory_file_system()
  fs.mkdir("/workspace").unwrap()
  fs.write_file("/workspace/app.txt", "hello").unwrap()
  let result = fs.delete("/workspace")
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="File system error: directory not empty: /workspace",
  )
}
