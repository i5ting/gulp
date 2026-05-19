///|
fn test_context() -> @core.Context {
  @core.new_context(
    cwd="/tmp",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
}

///|
fn write_text_fixture(root : String, rel : String, text : String) -> Unit {
  let input = @vinyl.vinyl_file(
    cwd=root,
    base=root,
    path=Some(root + "/" + rel),
    contents=@vinyl.Buffer(@utf8.encode(text)),
  )
  ignore(
    vinyl_file_stream([input]).pipe(dest(root)).collect(test_context()),
  )
}

///|
fn write_buffer_fixture(root : String, rel : String, bytes : Bytes) -> Unit {
  let input = @vinyl.vinyl_file(
    cwd=root,
    base=root,
    path=Some(root + "/" + rel),
    contents=@vinyl.Buffer(bytes),
  )
  ignore(
    vinyl_file_stream([input]).pipe(dest(root)).collect(test_context()),
  )
}

///|
fn write_directory_fixture(root : String, rel : String) -> Unit {
  let input = @vinyl.vinyl_file(
    cwd=root,
    base=root,
    path=Some(root + "/" + rel),
    contents=@vinyl.Null,
    stat=Some(@vinyl.vinyl_stat(kind=@vinyl.Directory)),
  )
  ignore(
    vinyl_file_stream([input]).pipe(dest(root)).collect(test_context()),
  )
}

///|
fn write_symlink_fixture(root : String, rel : String, target : String) -> Unit {
  let input = @vinyl.vinyl_file(
    cwd=root,
    base=root,
    path=Some(root + "/" + rel),
    contents=@vinyl.Null,
    stat=Some(@vinyl.vinyl_stat(kind=@vinyl.SymbolicLink)),
    symlink=Some(target),
  )
  ignore(
    vinyl_file_stream([input]).pipe(dest(root)).collect(test_context()),
  )
}

///|
fn find_relative(
  files : Array[@vinyl.VinylFile],
  rel : String,
) -> @vinyl.VinylFile? {
  for file in files {
    if file.relative() == rel {
      return Some(file)
    }
  }
  None
}

///|
fn text_contents(file : @vinyl.VinylFile) -> String {
  match file.contents() {
    @vinyl.Buffer(bytes) => @utf8.decode_lossy(bytes)
    @vinyl.Stream(stream) => stream.read_all(max_bytes=1024).unwrap()
    _ => ""
  }
}

///|
test "src reads matched files from cwd using vinyl relative paths" {
  let root = "/tmp/mulp-vinyl-fs-src"
  write_text_fixture(root, "a.txt", "alpha")
  write_text_fixture(root, "nested/b.txt", "beta")
  write_text_fixture(root, ".hidden.txt", "hidden")
  write_text_fixture(root, "skip.log", "skip")
  let options = src_options(cwd=root)
  let files = src(["**/*.txt"], options).collect(test_context()).unwrap()
  inspect(files.length(), content="2")
  inspect(find_relative(files, "a.txt").unwrap().is_buffer(), content="true")
  inspect(
    text_contents(find_relative(files, "a.txt").unwrap()),
    content="alpha",
  )
  inspect(
    text_contents(find_relative(files, "nested/b.txt").unwrap()),
    content="beta",
  )
}

///|
test "src reports invalid empty globs and missing literal files" {
  let root = "/tmp/mulp-vinyl-fs-src-invalid"
  write_text_fixture(root, "present.txt", "present")
  let empty_globs = src([], src_options(cwd=root)).collect(test_context())
  inspect(empty_globs is Err(_), content="true")
  let empty_glob = src([""], src_options(cwd=root)).collect(test_context())
  inspect(empty_glob is Err(_), content="true")
  let missing = src(["missing.txt"], src_options(cwd=root)).collect(
    test_context(),
  )
  inspect(missing is Err(_), content="true")
  let present = src(["present.txt"], src_options(cwd=root))
    .collect(test_context())
    .unwrap()
  inspect(present.length(), content="1")
}

///|
test "src allowEmpty permits missing literal files" {
  let root = "/tmp/mulp-vinyl-fs-src-allow-empty"
  write_text_fixture(root, "present.txt", "present")
  let files = src(
      ["missing.txt"],
      src_options(cwd=root, allow_empty=true),
    )
    .collect(test_context())
    .unwrap()
  inspect(files.length(), content="0")
}

///|
test "src supports negated globs and dot option" {
  let root = "/tmp/mulp-vinyl-fs-src-negated"
  write_text_fixture(root, "a.txt", "alpha")
  write_text_fixture(root, "nested/b.txt", "beta")
  write_text_fixture(root, ".hidden.txt", "hidden")
  let options = src_options(cwd=root, dot=true)
  let files = src(["**/*.txt", "!nested/*.txt"], options)
    .collect(test_context())
    .unwrap()
  inspect(files.length(), content="2")
  inspect(find_relative(files, "a.txt") is Some(_), content="true")
  inspect(find_relative(files, ".hidden.txt") is Some(_), content="true")
  inspect(find_relative(files, "nested/b.txt") is None, content="true")
}

///|
test "src applies negated globs in order" {
  let root = "/tmp/mulp-vinyl-fs-src-ordered-globs"
  write_text_fixture(root, "a.txt", "alpha")
  write_text_fixture(root, "b.txt", "beta")
  let options = src_options(cwd=root)
  let early_negation = src(["!b.txt", "*.txt"], options)
    .collect(test_context())
    .unwrap()
  inspect(early_negation.length(), content="2")
  inspect(find_relative(early_negation, "b.txt") is Some(_), content="true")
  let late_negation = src(["*.txt", "!b.txt"], options)
    .collect(test_context())
    .unwrap()
  inspect(late_negation.length(), content="1")
  inspect(find_relative(late_negation, "b.txt") is None, content="true")
}

///|
test "src streams directories with null contents and directory stats" {
  let root = "/tmp/mulp-vinyl-fs-src-directory"
  write_directory_fixture(root, "nested")
  let files = src(["nested"], src_options(cwd=root))
    .collect(test_context())
    .unwrap()
  inspect(files.length(), content="1")
  inspect(files[0].relative(), content="nested")
  inspect(files[0].is_directory(), content="true")
  inspect(files[0].is_null(), content="true")
}

///|
test "src filters files changed before since" {
  let root = "/tmp/mulp-vinyl-fs-src-since"
  write_text_fixture(root, "fresh.txt", "fresh")
  let initial = src(["fresh.txt"], src_options(cwd=root))
    .collect(test_context())
    .unwrap()
  let mtime = initial[0].stat().unwrap().mtime_ms().unwrap()
  let before = src(["fresh.txt"], src_options(cwd=root, since=Some(mtime - 1L)))
    .collect(test_context())
    .unwrap()
  inspect(before.length(), content="1")
  let after = src(["fresh.txt"], src_options(cwd=root, since=Some(mtime + 1L)))
    .collect(test_context())
    .unwrap()
  inspect(after.length(), content="0")
}

///|
#cfg(not(platform="windows"))
test "src includes files with ctime after since" {
  let root = "/tmp/mulp-vinyl-fs-src-since-ctime"
  let output_path = root + "/stale-mtime.txt"
  ignore(remove_path_for_test(output_path))
  let old_mtime = 123456000L
  let input = @vinyl.vinyl_file(
    cwd="/workspace",
    base="/workspace/src",
    path=Some("/workspace/src/stale-mtime.txt"),
    contents=@vinyl.Buffer(@utf8.encode("changed metadata")),
    stat=Some(@vinyl.vinyl_stat(kind=@vinyl.File, mtime_ms=Some(old_mtime))),
  )
  ignore(
    vinyl_file_stream([input])
    .pipe(dest(root))
    .collect(test_context()),
  )
  let files = src(["stale-mtime.txt"], src_options(cwd=root, since=Some(old_mtime + 1L)))
    .collect(test_context())
    .unwrap()
  inspect(files.length(), content="1")
}

///|
test "src can expose file contents as byte streams" {
  let root = "/tmp/mulp-vinyl-fs-src-stream"
  write_text_fixture(root, "a.txt", "streamed")
  let options = src_options(cwd=root, buffer=false)
  let files = src(["*.txt"], options).collect(test_context()).unwrap()
  match files[0].contents() {
    @vinyl.Stream(bytes) =>
      inspect(bytes.read_all(max_bytes=32).unwrap(), content="streamed")
    _ => inspect("wrong contents", content="byte stream")
  }
}

///|
test "src strips utf8 bom from buffered files by default" {
  let root = "/tmp/mulp-vinyl-fs-src-bom"
  write_buffer_fixture(
    root,
    "bom.txt",
    Bytes::from_array([
      b'\xef', b'\xbb', b'\xbf', b'h', b'e', b'l', b'l', b'o',
    ]),
  )
  let files = src(["bom.txt"], src_options(cwd=root))
    .collect(test_context())
    .unwrap()
  inspect(text_contents(files[0]), content="hello")
}

///|
test "src preserves utf8 bom when removeBOM is false" {
  let root = "/tmp/mulp-vinyl-fs-src-preserve-bom"
  let bytes = Bytes::from_array([b'\xef', b'\xbb', b'\xbf', b'a'])
  write_buffer_fixture(root, "bom.txt", bytes)
  let files = src(["bom.txt"], src_options(cwd=root, remove_bom=false))
    .collect(test_context())
    .unwrap()
  match files[0].contents() {
    @vinyl.Buffer(contents) => inspect(contents, content=bytes.to_string())
    _ => inspect("wrong contents", content="Buffer")
  }
}

///|
test "src preserves raw bytes when encoding is false" {
  let root = "/tmp/mulp-vinyl-fs-src-encoding-false"
  let bytes = Bytes::from_array([b'\xef', b'\xbb', b'\xbf', b'a'])
  write_buffer_fixture(root, "bom.txt", bytes)
  let files = src(["bom.txt"], src_options(cwd=root, encoding=Raw))
    .collect(test_context())
    .unwrap()
  match files[0].contents() {
    @vinyl.Buffer(contents) => inspect(contents, content=bytes.to_string())
    _ => inspect("wrong contents", content="Buffer")
  }
}

///|
test "src attaches external sourcemap files when sourcemaps is true" {
  let root = "/tmp/mulp-vinyl-fs-src-sourcemaps"
  write_text_fixture(root, "main.js", "console.log(1)")
  write_text_fixture(root, "main.js.map", "{\"version\":3}")
  let files = src(["main.js"], src_options(cwd=root, sourcemaps=true))
    .collect(test_context())
    .unwrap()
  inspect(files.length(), content="1")
  inspect(files[0].source_map().unwrap(), content="{\"version\":3}")
}

///|
test "src resolved symlinks expose target file metadata" {
  if src_symlink_supported_for_test() {
    let root = "/tmp/mulp-vinyl-fs-src-resolved-symlink"
    write_text_fixture(root, "target.txt", "target")
    write_symlink_fixture(root, "link.txt", root + "/target.txt")
    let files = src(["link.txt"], src_options(cwd=root))
      .collect(test_context())
      .unwrap()
    inspect(files.length(), content="1")
    inspect(files[0].stat().unwrap().kind() is @vinyl.File, content="true")
    inspect(text_contents(files[0]), content="target")
  } else {
    inspect(src_symlink_supported_for_test(), content="false")
  }
}

///|
test "src can preserve symlinks when resolve symlinks is false" {
  if src_symlink_supported_for_test() {
    let root = "/tmp/mulp-vinyl-fs-src-preserve-symlink"
    write_text_fixture(root, "target.txt", "target")
    write_symlink_fixture(root, "link.txt", root + "/target.txt")
    let files = src(["link.txt"], src_options(cwd=root, resolve_symlinks=false))
      .collect(test_context())
      .unwrap()
    inspect(files.length(), content="1")
    inspect(files[0].is_symbolic(), content="true")
    inspect(files[0].symlink().unwrap(), content=root + "/target.txt")
    inspect(files[0].is_null(), content="true")
  } else {
    inspect(src_symlink_supported_for_test(), content="false")
  }
}

///|
extern "c" fn src_symlink_supported_for_test() -> Bool = "mulp_symlink_supported_for_test"
