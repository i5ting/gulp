///|
// Test parity notes for vinyl-master/test/file.js:
// - MoonBit uses typed values instead of JS dynamic objects, so invalid setter
//   value tests, private _field proxy tests, isVinyl(false) dynamic cases, and
//   prototype inheritance tests are documented as not applicable here.
// - JS property setters are represented by immutable with_* methods; setter
//   throws are represented by *_result methods where a missing path is possible.
// - JS object custom properties are represented by metadata key/value pairs.
fn expect_path(file : VinylFile, expected : String) -> Unit raise {
  inspect(file.path().unwrap(), content=expected)
}

///|
fn expect_no_path(file : VinylFile) -> Unit raise {
  inspect(file.path() is None, content="true")
}

///|
fn expect_history(file : VinylFile, expected : String) -> Unit raise {
  inspect(file.history(), content=expected)
}

///|
fn expect_buffer(file : VinylFile, expected : Bytes) -> Unit raise {
  match file.contents() {
    Buffer(bytes) => inspect(bytes, content=expected.to_string())
    _ => inspect("wrong contents", content="Buffer")
  }
}

///|
fn expect_stream_text(file : VinylFile, expected : String) -> Unit raise {
  match file.contents() {
    Stream(stream) =>
      inspect(stream.read_all(max_bytes=1024).unwrap(), content=expected)
    _ => inspect("wrong contents", content="Stream")
  }
}

///|
test "vinyl-master isVinyl returns true for a Vinyl object" {
  inspect(is_vinyl(vinyl_file()), content="true")
}

///|
test "vinyl-master defaults cwd to process cwd" {
  inspect(vinyl_file().cwd(), content=@env.current_dir().unwrap())
}

///|
test "vinyl-master defaults base to process cwd" {
  inspect(vinyl_file().base(), content=@env.current_dir().unwrap())
}

///|
test "vinyl-master defaults base to cwd property" {
  inspect(vinyl_file(cwd="/").base(), content="/")
}

///|
test "vinyl-master defaults path to null" {
  expect_no_path(vinyl_file())
}

///|
test "vinyl-master defaults history to an empty array" {
  expect_history(vinyl_file(), "[]")
}

///|
test "vinyl-master defaults stat to null" {
  inspect(vinyl_file().stat() is None, content="true")
}

///|
test "vinyl-master defaults contents to null" {
  inspect(vinyl_file().is_null(), content="true")
}

///|
test "vinyl-master constructor sets base" {
  inspect(vinyl_file(base="/").base(), content="/")
}

///|
test "vinyl-master constructor sets cwd" {
  inspect(vinyl_file(cwd="/").cwd(), content="/")
}

///|
test "vinyl-master constructor sets path and history" {
  let file = vinyl_file(path=Some("/test.coffee"))
  expect_path(file, "/test.coffee")
  expect_history(file, "[\"/test.coffee\"]")
}

///|
test "vinyl-master constructor sets history and path" {
  let file = vinyl_file(history=["/test.coffee"])
  expect_path(file, "/test.coffee")
  expect_history(file, "[\"/test.coffee\"]")
}

///|
test "vinyl-master constructor sets stat" {
  let stat = vinyl_stat(kind=File, mode=Some(0o644), mtime_ms=Some(1000L))
  let file = vinyl_file(stat=Some(stat))
  inspect(file.stat().unwrap().mode().unwrap(), content="420")
  inspect(file.stat().unwrap().mtime_ms().unwrap(), content="1000")
}

///|
test "vinyl-master constructor sets contents" {
  expect_buffer(vinyl_file(contents=Buffer(b"test")), b"test")
}

///|
test "vinyl-master constructor sets custom properties as metadata" {
  let file = vinyl_file(metadata=[("sourceMap", "{\"version\":3}")])
  inspect(file.metadata_value("sourceMap").unwrap(), content="{\"version\":3}")
}

///|
test "vinyl-master constructor sets sourceMap as a first class field" {
  let file = vinyl_file(source_map=Some("{\"version\":3}"))
  inspect(file.source_map().unwrap(), content="{\"version\":3}")
}

///|
test "vinyl-master isCustomProp matches built in field names" {
  inspect(is_custom_prop("custom"), content="true")
  inspect(is_custom_prop("sourceMap"), content="true")
  inspect(is_custom_prop("contents"), content="false")
  inspect(is_custom_prop("_cwd"), content="false")
}

///|
test "vinyl-master constructor normalizes path" {
  let file = vinyl_file(path=Some("/test/foo/../test.coffee"))
  expect_path(file, "/test/test.coffee")
  expect_history(file, "[\"/test/test.coffee\"]")
}

///|
test "vinyl-master constructor normalizes and removes trailing separator from path" {
  expect_path(vinyl_file(path=Some("/test/foo/../foo/")), "/test/foo")
}

///|
test "vinyl-master constructor normalizes history" {
  let file = vinyl_file(history=[
    "/test/bar/../bar/test.coffee", "/test/foo/../test.coffee",
  ])
  expect_path(file, "/test/test.coffee")
  expect_history(file, "[\"/test/bar/test.coffee\", \"/test/test.coffee\"]")
}

///|
test "vinyl-master constructor normalizes and removes trailing separator from history" {
  let file = vinyl_file(history=["/test/foo/../foo/", "/test/bar/../bar/"])
  expect_history(file, "[\"/test/foo\", \"/test/bar\"]")
}

///|
test "vinyl-master constructor appends path to history if both exist and different from last" {
  let file = vinyl_file(path=Some("/test/baz/test.coffee"), history=[
    "/test/bar/test.coffee", "/test/foo/test.coffee",
  ])
  expect_path(file, "/test/baz/test.coffee")
  expect_history(
    file, "[\"/test/bar/test.coffee\", \"/test/foo/test.coffee\", \"/test/baz/test.coffee\"]",
  )
}

///|
test "vinyl-master constructor does not append path to history if both exist and same as last" {
  let file = vinyl_file(path=Some("/test/baz/test.coffee"), history=[
    "/test/bar/test.coffee", "/test/foo/test.coffee", "/test/baz/test.coffee",
  ])
  expect_history(
    file, "[\"/test/bar/test.coffee\", \"/test/foo/test.coffee\", \"/test/baz/test.coffee\"]",
  )
}

///|
test "vinyl-master constructor does not mutate history array passed in" {
  let history = ["/test/bar/test.coffee", "/test/foo/test.coffee"]
  let file = vinyl_file(path=Some("/test/baz/test.coffee"), history~)
  expect_history(
    file, "[\"/test/bar/test.coffee\", \"/test/foo/test.coffee\", \"/test/baz/test.coffee\"]",
  )
  inspect(
    history,
    content="[\"/test/bar/test.coffee\", \"/test/foo/test.coffee\"]",
  )
}

///|
test "vinyl-master isBuffer returns true when contents are a Buffer" {
  inspect(vinyl_file(contents=Buffer(b"test")).is_buffer(), content="true")
}

///|
test "vinyl-master isBuffer returns false when contents are a Stream" {
  inspect(
    vinyl_file(contents=Stream(@stream.byte_stream([]))).is_buffer(),
    content="false",
  )
}

///|
test "vinyl-master isBuffer returns false when contents are null" {
  inspect(vinyl_file(contents=Null).is_buffer(), content="false")
}

///|
test "vinyl-master isStream returns false when contents are a Buffer" {
  inspect(vinyl_file(contents=Buffer(b"test")).is_stream(), content="false")
}

///|
test "vinyl-master isStream returns true when contents are a Stream" {
  inspect(
    vinyl_file(contents=Stream(@stream.byte_stream([]))).is_stream(),
    content="true",
  )
}

///|
test "vinyl-master isStream returns false when contents are null" {
  inspect(vinyl_file(contents=Null).is_stream(), content="false")
}

///|
test "vinyl-master isNull returns false when contents are a Buffer" {
  inspect(vinyl_file(contents=Buffer(b"test")).is_null(), content="false")
}

///|
test "vinyl-master isNull returns false when contents are a Stream" {
  inspect(
    vinyl_file(contents=Stream(@stream.byte_stream([]))).is_null(),
    content="false",
  )
}

///|
test "vinyl-master isNull returns true when contents are null" {
  inspect(vinyl_file(contents=Null).is_null(), content="true")
}

///|
test "vinyl-master isDirectory returns false when contents are a Buffer" {
  let stat = Some(vinyl_stat(kind=Directory))
  inspect(
    vinyl_file(contents=Buffer(b"test"), stat~).is_directory(),
    content="false",
  )
}

///|
test "vinyl-master isDirectory returns false when contents are a Stream" {
  let stat = Some(vinyl_stat(kind=Directory))
  inspect(
    vinyl_file(contents=Stream(@stream.byte_stream([])), stat~).is_directory(),
    content="false",
  )
}

///|
test "vinyl-master isDirectory returns true when contents are null and stat is directory" {
  inspect(
    vinyl_file(contents=Null, stat=Some(vinyl_stat(kind=Directory))).is_directory(),
    content="true",
  )
}

///|
test "vinyl-master isDirectory returns false when stat exists but is not directory" {
  inspect(
    vinyl_file(contents=Null, stat=Some(vinyl_stat(kind=File))).is_directory(),
    content="false",
  )
}

///|
test "vinyl-master isDirectory returns false when stat does not exist" {
  inspect(vinyl_file(contents=Null).is_directory(), content="false")
}

///|
test "vinyl-master isSymbolic returns false when contents are a Buffer" {
  let stat = Some(vinyl_stat(kind=SymbolicLink))
  inspect(
    vinyl_file(contents=Buffer(b"test"), stat~).is_symbolic(),
    content="false",
  )
}

///|
test "vinyl-master isSymbolic returns false when contents are a Stream" {
  let stat = Some(vinyl_stat(kind=SymbolicLink))
  inspect(
    vinyl_file(contents=Stream(@stream.byte_stream([])), stat~).is_symbolic(),
    content="false",
  )
}

///|
test "vinyl-master isSymbolic returns true when contents are null and stat is symbolic link" {
  inspect(
    vinyl_file(contents=Null, stat=Some(vinyl_stat(kind=SymbolicLink))).is_symbolic(),
    content="true",
  )
}

///|
test "vinyl-master isSymbolic returns false when stat exists but is not symbolic link" {
  inspect(
    vinyl_file(contents=Null, stat=Some(vinyl_stat(kind=File))).is_symbolic(),
    content="false",
  )
}

///|
test "vinyl-master isSymbolic returns false when stat does not exist" {
  inspect(vinyl_file(contents=Null).is_symbolic(), content="false")
}

///|
test "vinyl-master clone copies all attributes over with Buffer contents" {
  let file = vinyl_file(
    cwd="/",
    base="/test/",
    path=Some("/test/test.coffee"),
    contents=Buffer(b"test"),
  )
  let copied = file.clone()
  inspect(copied.cwd(), content=file.cwd())
  inspect(copied.base(), content=file.base())
  inspect(copied.path().unwrap(), content=file.path().unwrap())
  expect_buffer(copied, b"test")
}

///|
test "vinyl-master clone assigns Buffer content reference when contents option is false" {
  let file = vinyl_file(contents=Buffer(b"test"))
  expect_buffer(file.clone(contents=false), b"test")
}

///|
test "vinyl-master clone copies all attributes over with Stream contents" {
  let file = vinyl_file(
    cwd="/",
    base="/test/",
    path=Some("/test/test.coffee"),
    contents=Stream(@stream.byte_stream(["wa", "dup"])),
  )
  let copied = file.clone()
  inspect(copied.cwd(), content=file.cwd())
  inspect(copied.base(), content=file.base())
  inspect(copied.path().unwrap(), content=file.path().unwrap())
  expect_stream_text(file, "wadup")
  expect_stream_text(copied, "wadup")
}

///|
test "vinyl-master clone does not lose data when clones flow differently data" {
  let file = vinyl_file(contents=Stream(@stream.byte_stream(["wa", "dup"])))
  let copied = file.clone()
  expect_stream_text(copied, "wadup")
  expect_stream_text(file, "wadup")
}

///|
test "vinyl-master clone does not lose data when clones flow differently readable and pipe" {
  let file = vinyl_file(contents=Stream(@stream.byte_stream(["wa", "dup"])))
  let copied = file.clone()
  expect_stream_text(file, "wadup")
  expect_stream_text(copied, "wadup")
}

///|
test "vinyl-master clone contents false still tees stream contents" {
  let file = vinyl_file(contents=Stream(@stream.byte_stream(["wa", "dup"])))
  let copied = file.clone(contents=false)
  expect_stream_text(copied, "wadup")
  expect_stream_text(file, "wadup")
}

///|
test "vinyl-master clone fixes file symlink if file is a symbolic link" {
  let file = vinyl_file(
    cwd="/",
    base="/test/",
    path=Some("/test/test.coffee"),
    contents=Null,
    stat=Some(vinyl_stat(kind=SymbolicLink)),
    symlink=Some("/test/test.js"),
  )
  inspect(file.clone().symlink().unwrap(), content="/test/test.js")
}

///|
test "vinyl-master clone copies all attributes over with null contents" {
  let file = vinyl_file(
    cwd="/",
    base="/test/",
    path=Some("/test/test.coffee"),
    contents=Null,
  )
  let copied = file.clone()
  inspect(copied.cwd(), content=file.cwd())
  inspect(copied.base(), content=file.base())
  inspect(copied.path().unwrap(), content=file.path().unwrap())
  inspect(copied.is_null(), content="true")
}

///|
test "vinyl-master clone properly clones the stat property" {
  let file = vinyl_file(stat=Some(vinyl_stat(kind=File, mode=Some(0o644))))
  let copied = file.clone()
  inspect(copied.stat().unwrap().kind() is File, content="true")
  inspect(copied.stat().unwrap().mode().unwrap(), content="420")
}

///|
test "vinyl-master clone properly clones the history property" {
  let file = vinyl_file(
    cwd="/",
    base="/test/",
    path=Some("/test/test.js"),
    contents=Buffer(b"test"),
  )
  let copied = file.clone().with_path("lol")
  expect_path(file, "/test/test.js")
  expect_path(copied, "lol")
}

///|
test "vinyl-master clone copies custom properties as metadata" {
  let file = vinyl_file(metadata=[("custom", "{\"meta\":{}}")])
  inspect(
    file.clone().metadata_value("custom").unwrap(),
    content="{\"meta\":{}}",
  )
}

///|
test "vinyl-master clone copies sourceMap" {
  let file = vinyl_file(source_map=Some("{\"version\":3}"))
  inspect(file.clone().source_map().unwrap(), content="{\"version\":3}")
}

///|
test "vinyl-master clone copies history" {
  let file = vinyl_file(
      cwd="/",
      base="/test/",
      path=Some("/test/test.coffee"),
      contents=Null,
    )
    .with_path("/test/test.js")
    .with_path("/test/test-938di2s.js")
  let copied = file.clone()
  expect_history(
    copied, "[\"/test/test.coffee\", \"/test/test.js\", \"/test/test-938di2s.js\"]",
  )
  expect_path(copied, "/test/test-938di2s.js")
}

///|
test "vinyl-master clone supports deep and shallow copy of all attributes" {
  let file = vinyl_file(metadata=[("custom", "{\"meta\":{}}")])
  inspect(file.clone().metadata(), content=file.metadata().to_string())
  inspect(file.clone(deep=true).metadata(), content=file.metadata().to_string())
  inspect(
    file.clone(deep=false).metadata(),
    content=file.metadata().to_string(),
  )
}

///|
test "vinyl-master inspect returns correct format when no contents and no path" {
  inspect(vinyl_file().inspect(), content="<File >")
}

///|
test "vinyl-master inspect returns correct format when Buffer contents and no path" {
  inspect(
    vinyl_file(contents=Buffer(b"test")).inspect(),
    content="<File <Buffer 74 65 73 74>>",
  )
}

///|
test "vinyl-master inspect returns correct format when Buffer contents and relative path" {
  inspect(
    vinyl_file(
      cwd="/",
      base="/test/",
      path=Some("/test/test.coffee"),
      contents=Buffer(b"test"),
    ).inspect(),
    content="<File \"test.coffee\" <Buffer 74 65 73 74>>",
  )
}

///|
test "vinyl-master inspect returns correct format when Stream contents and relative path" {
  inspect(
    vinyl_file(
      cwd="/",
      base="/test/",
      path=Some("/test/test.coffee"),
      contents=Stream(@stream.byte_stream([])),
    ).inspect(),
    content="<File \"test.coffee\" <ReadableStream>>",
  )
}

///|
test "vinyl-master inspect returns correct format when null contents and relative path" {
  inspect(
    vinyl_file(
      cwd="/",
      base="/test/",
      path=Some("/test/test.coffee"),
      contents=Null,
    ).inspect(),
    content="<File \"test.coffee\">",
  )
}

///|
test "vinyl-master contents setter sets a Buffer" {
  expect_buffer(vinyl_file().with_contents(Buffer(b"test")), b"test")
}

///|
test "vinyl-master contents setter sets null" {
  inspect(
    vinyl_file(contents=Buffer(b"test")).with_contents(Null).is_null(),
    content="true",
  )
}

///|
test "vinyl-master cwd setter sets cwd" {
  inspect(vinyl_file().with_cwd("/test").cwd(), content="/test")
}

///|
test "vinyl-master cwd setter normalizes and removes trailing separator on set" {
  inspect(vinyl_file().with_cwd("/test/foo/../foo/").cwd(), content="/test/foo")
}

///|
test "vinyl-master base getter proxies cwd when omitted" {
  let file = vinyl_file(cwd="/test")
  inspect(file.base(), content=file.cwd())
}

///|
test "vinyl-master base getter proxies cwd when same" {
  let file = vinyl_file(cwd="/test", base="/test").with_cwd("/foo/")
  inspect(file.base(), content=file.cwd())
  let file2 = vinyl_file(cwd="/test").with_base("/test/").with_cwd("/foo/")
  inspect(file2.base(), content=file2.cwd())
}

///|
test "vinyl-master base setter proxies to cwd when set to same value" {
  let file = vinyl_file(cwd="/foo", base="/bar")
  inspect(file.base() == file.cwd(), content="false")
  let same = file.with_base(file.cwd())
  inspect(same.base(), content=same.cwd())
}

///|
test "vinyl-master base setter proxies to cwd when cleared" {
  let file = vinyl_file(cwd="/foo", base="/bar")
  inspect(file.base() == file.cwd(), content="false")
  let cleared = file.with_base_default()
  inspect(cleared.base(), content=cleared.cwd())
  let moved = cleared.with_cwd("/baz")
  inspect(moved.base(), content=moved.cwd())
}

///|
test "vinyl-master base setter sets base" {
  inspect(vinyl_file().with_base("/test/foo").base(), content="/test/foo")
}

///|
test "vinyl-master base setter normalizes and removes trailing separator on set" {
  inspect(
    vinyl_file().with_base("/test/foo/../foo/").base(),
    content="/test/foo",
  )
}

///|
test "vinyl-master relative setter is not represented by immutable API" {
  inspect(vinyl_file().relative_result() is Err(_), content="true")
}

///|
test "vinyl-master relative getter throws on get with no path" {
  inspect(
    vinyl_file().relative_result().unwrap_err(),
    content="No path specified! Can not get relative.",
  )
}

///|
test "vinyl-master relative returns a relative path from base" {
  inspect(
    vinyl_file(base="/test/", path=Some("/test/test.coffee")).relative(),
    content="test.coffee",
  )
}

///|
test "vinyl-master relative returns a relative path from cwd" {
  inspect(
    vinyl_file(cwd="/", path=Some("/test/test.coffee")).relative(),
    content="test/test.coffee",
  )
}

///|
test "vinyl-master relative does not append separator when directory" {
  inspect(
    vinyl_file(
      base="/test",
      path=Some("/test/foo/bar"),
      stat=Some(vinyl_stat(kind=Directory)),
    ).relative(),
    content="foo/bar",
  )
}

///|
test "vinyl-master relative does not append separator when symlink" {
  inspect(
    vinyl_file(
      base="/test",
      path=Some("/test/foo/bar"),
      stat=Some(vinyl_stat(kind=SymbolicLink)),
    ).relative(),
    content="foo/bar",
  )
}

///|
test "vinyl-master dirname getter throws on get with no path" {
  inspect(
    vinyl_file().dirname_result().unwrap_err(),
    content="No path specified! Can not get dirname.",
  )
}

///|
test "vinyl-master dirname returns the dirname without trailing separator" {
  inspect(
    vinyl_file(cwd="/", base="/test", path=Some("/test/test.coffee")).dirname(),
    content="/test",
  )
}

///|
test "vinyl-master dirname setter throws on set with no path" {
  inspect(
    vinyl_file().with_dirname_result("/test").unwrap_err(),
    content="No path specified! Can not set dirname.",
  )
}

///|
test "vinyl-master dirname setter replaces the dirname of the path" {
  expect_path(
    vinyl_file(cwd="/", base="/test/", path=Some("/test/test.coffee")).with_dirname(
      "/test/foo",
    ),
    "/test/foo/test.coffee",
  )
}

///|
test "vinyl-master basename getter throws on get with no path" {
  inspect(
    vinyl_file().basename_result().unwrap_err(),
    content="No path specified! Can not get basename.",
  )
}

///|
test "vinyl-master basename returns the basename of the path" {
  inspect(
    vinyl_file(cwd="/", base="/test/", path=Some("/test/test.coffee")).basename(),
    content="test.coffee",
  )
}

///|
test "vinyl-master basename does not append trailing separator when directory" {
  inspect(
    vinyl_file(path=Some("/test/foo"), stat=Some(vinyl_stat(kind=Directory))).basename(),
    content="foo",
  )
}

///|
test "vinyl-master basename does not append trailing separator when symlink" {
  inspect(
    vinyl_file(path=Some("/test/foo"), stat=Some(vinyl_stat(kind=SymbolicLink))).basename(),
    content="foo",
  )
}

///|
test "vinyl-master basename removes trailing separator" {
  inspect(vinyl_file(path=Some("/test/foo/")).basename(), content="foo")
}

///|
test "vinyl-master basename setter throws on set with no path" {
  inspect(
    vinyl_file().with_basename_result("test.coffee").unwrap_err(),
    content="No path specified! Can not set basename.",
  )
}

///|
test "vinyl-master basename setter replaces the basename of the path" {
  expect_path(
    vinyl_file(cwd="/", base="/test/", path=Some("/test/test.coffee")).with_basename(
      "foo.png",
    ),
    "/test/foo.png",
  )
}

///|
test "vinyl-master stem getter throws on get with no path" {
  inspect(
    vinyl_file().stem_result().unwrap_err(),
    content="No path specified! Can not get stem.",
  )
}

///|
test "vinyl-master stem returns the stem of the path" {
  inspect(
    vinyl_file(cwd="/", base="/test/", path=Some("/test/test.coffee")).stem(),
    content="test",
  )
}

///|
test "vinyl-master stem setter throws on set with no path" {
  inspect(
    vinyl_file().with_stem_result("test").unwrap_err(),
    content="No path specified! Can not set stem.",
  )
}

///|
test "vinyl-master stem setter replaces the stem of the path" {
  expect_path(
    vinyl_file(cwd="/", base="/test/", path=Some("/test/test.coffee")).with_stem(
      "foo",
    ),
    "/test/foo.coffee",
  )
}

///|
test "vinyl-master extname getter throws on get with no path" {
  inspect(
    vinyl_file().extname_result().unwrap_err(),
    content="No path specified! Can not get extname.",
  )
}

///|
test "vinyl-master extname returns the extname of the path" {
  inspect(
    vinyl_file(cwd="/", base="/test/", path=Some("/test/test.coffee")).extname(),
    content=".coffee",
  )
}

///|
test "vinyl-master extname setter throws on set with no path" {
  inspect(
    vinyl_file().with_extname_result(".coffee").unwrap_err(),
    content="No path specified! Can not set extname.",
  )
}

///|
test "vinyl-master extname setter replaces the extname of the path" {
  expect_path(
    vinyl_file(cwd="/", base="/test/", path=Some("/test/test.coffee")).with_extname(
      ".png",
    ),
    "/test/test.png",
  )
}

///|
test "vinyl-master path records path in history upon instantiation" {
  let file = vinyl_file(cwd="/", path=Some("/test/test.coffee"))
  expect_path(file, "/test/test.coffee")
  expect_history(file, "[\"/test/test.coffee\"]")
}

///|
test "vinyl-master path records path in history when set" {
  let file = vinyl_file(cwd="/", path=Some("/test/test.coffee"))
    .with_path("/test/test.js")
    .with_path("/test/test.es6")
  expect_path(file, "/test/test.es6")
  expect_history(
    file, "[\"/test/test.coffee\", \"/test/test.js\", \"/test/test.es6\"]",
  )
}

///|
test "vinyl-master path does not record path in history when set to the current path" {
  let file = vinyl_file(cwd="/", path=Some("/test/test.coffee"))
    .with_path("/test/test.coffee")
    .with_path("/test/test.coffee")
  expect_history(file, "[\"/test/test.coffee\"]")
}

///|
test "vinyl-master path does not record path in history when set to empty string" {
  let file = vinyl_file(cwd="/", path=Some("/test/test.coffee")).with_path("")
  expect_path(file, "/test/test.coffee")
  expect_history(file, "[\"/test/test.coffee\"]")
}

///|
test "vinyl-master path normalizes the path upon set" {
  let file = vinyl_file().with_path("/test/foo/../test.coffee")
  expect_path(file, "/test/test.coffee")
  expect_history(file, "[\"/test/test.coffee\"]")
}

///|
test "vinyl-master path removes the trailing separator upon set" {
  let file = vinyl_file().with_path("/test/")
  expect_path(file, "/test")
  expect_history(file, "[\"/test\"]")
}

///|
test "vinyl-master symlink returns null on get with no symlink" {
  inspect(vinyl_file().symlink() is None, content="true")
}

///|
test "vinyl-master symlink setter sets symlink" {
  inspect(
    vinyl_file().with_symlink("/test/test.coffee").symlink().unwrap(),
    content="/test/test.coffee",
  )
}

///|
test "vinyl-master symlink setter allows relative symlink" {
  inspect(
    vinyl_file().with_symlink("test.coffee").symlink().unwrap(),
    content="test.coffee",
  )
}

///|
test "vinyl-master symlink setter normalizes and removes trailing separator upon set" {
  inspect(
    vinyl_file().with_symlink("/test/foo/../bar/").symlink().unwrap(),
    content="/test/bar",
  )
}
