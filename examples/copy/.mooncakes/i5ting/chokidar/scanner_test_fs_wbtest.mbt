///|
#borrow(path)
extern "C" fn native_test_create_dir(path : Bytes) -> Int = "readdirp_test_create_dir"

///|
#borrow(path, contents)
extern "C" fn native_test_write_file(path : Bytes, contents : Bytes) -> Int = "readdirp_test_write_file"

///|
#borrow(path)
extern "C" fn native_test_remove_tree(path : Bytes) -> Int = "readdirp_test_remove_tree"

///|
fn test_path_to_bytes(path : String) -> Bytes {
  test_string_to_utf8_bytes(path)
}

///|
fn test_string_to_utf8_bytes(value : String) -> Bytes {
  let byte_length = value
    .iter()
    .fold(init=0, (length, char) => {
      let code = char.to_int()
      if code < 0x80 {
        length + 1
      } else if code < 0x800 {
        length + 2
      } else if code < 0x10000 {
        length + 3
      } else {
        length + 4
      }
    })
  let bytes = FixedArray::make(byte_length, b'\x00')
  ignore(
    value
    .iter()
    .fold(init=0, (offset, char) => offset + bytes.set_utf8_char(offset, char)),
  )
  test_fixed_byte_array_to_bytes(bytes)
}

///|
fn test_fixed_byte_array_to_bytes(bytes : FixedArray[Byte]) -> Bytes = "%identity"

///|
fn test_create_dir(path : String) -> Unit raise ChokidarError {
  guard native_test_create_dir(test_path_to_bytes(path)) == 0 else {
    raise ChokidarError("failed to create test directory: " + path)
  }
}

///|
fn test_write_file(
  path : String,
  contents : String,
) -> Unit raise ChokidarError {
  guard native_test_write_file(
      test_path_to_bytes(path),
      test_string_to_utf8_bytes(contents),
    ) ==
    0 else {
    raise ChokidarError("failed to write test file: " + path)
  }
}

///|
fn test_remove_tree(path : String) -> Unit raise ChokidarError {
  guard native_test_remove_tree(test_path_to_bytes(path)) == 0 else {
    raise ChokidarError("failed to remove test tree: " + path)
  }
}

///|
#borrow(target, link_path)
extern "C" fn native_test_symlink(
  target : Bytes,
  link_path : Bytes,
) -> Int = "readdirp_test_symlink"

///|
fn test_create_symlink(
  target : String,
  link_path : String,
) -> Unit raise ChokidarError {
  guard native_test_symlink(
      test_path_to_bytes(target),
      test_path_to_bytes(link_path),
    ) ==
    0 else {
    raise ChokidarError("failed to create symlink: " + link_path + " -> " + target)
  }
}

///|
#borrow(path)
extern "C" fn native_test_chmod(path : Bytes, mode : Int) -> Int = "chokidar_test_chmod"

///|
fn test_chmod(path : String, mode : Int) -> Unit raise ChokidarError {
  guard native_test_chmod(test_path_to_bytes(path), mode) == 0 else {
    raise ChokidarError("failed to chmod: " + path)
  }
}

///|
#borrow(name, value)
extern "C" fn native_test_setenv(name : Bytes, value : Bytes) -> Int = "chokidar_test_setenv"

///|
#borrow(name)
extern "C" fn native_test_unsetenv(name : Bytes) -> Int = "chokidar_test_unsetenv"

///|
fn test_setenv(name : String, value : String) -> Unit raise ChokidarError {
  guard native_test_setenv(test_path_to_bytes(name), test_path_to_bytes(value)) ==
    0 else {
    raise ChokidarError("failed to setenv: " + name)
  }
}

///|
fn test_unsetenv(name : String) -> Unit raise ChokidarError {
  guard native_test_unsetenv(test_path_to_bytes(name)) == 0 else {
    raise ChokidarError("failed to unsetenv: " + name)
  }
}

///|
fn test_sleep_millis(millis : Int) -> Unit {
  let deadline = @env.now() + millis.reinterpret_as_uint().to_uint64()
  while @env.now() < deadline {
    ()
  }
}
