///|
fn dest_test_context() -> @core.Context {
  @core.new_context(
    cwd="/tmp",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
}

///|
test "dest writes files under output directory and returns updated vinyl paths" {
  let input = @vinyl.vinyl_file(
    cwd="/workspace",
    base="/workspace/src",
    path=Some("/workspace/src/app/main.txt"),
    contents=@vinyl.Buffer(@utf8.encode("hello")),
  )
  let outputs = vinyl_file_stream([input])
    .pipe(dest("/tmp/mulp-vinyl-fs-dest"))
    .collect(dest_test_context())
    .unwrap()
  inspect(outputs.length(), content="1")
  inspect(
    outputs[0].path().unwrap(),
    content="/tmp/mulp-vinyl-fs-dest/app/main.txt",
  )
  inspect(outputs[0].base(), content="/tmp/mulp-vinyl-fs-dest")
  let written = @stream.file_byte_stream(
    "/tmp/mulp-vinyl-fs-dest/app/main.txt",
    chunk_size=16,
  ).unwrap()
  inspect(written.read_all(max_bytes=16).unwrap(), content="hello")
}

///|
test "dest accepts through2 vinyl transforms" {
  let out = "/tmp/mulp-vinyl-fs-through2-dest"
  let output_path = out + "/app.out.txt"
  ignore(remove_path_for_test(output_path))
  let input = @vinyl.vinyl_file(
    cwd="/workspace",
    base="/workspace/src",
    path=Some("/workspace/src/app.txt"),
    contents=@vinyl.Buffer(@utf8.encode("hello")),
  )
  let outputs = vinyl_file_stream([input])
    .pipe(
      @through2.through_vinyl(fn(
        file : @vinyl.VinylFile,
      ) -> Result[@vinyl.VinylFile?, @core.MulpError] {
        Ok(Some(file.with_path("/workspace/src/app.out.txt")))
      }),
    )
    .pipe(dest(out))
    .collect(dest_test_context())
    .unwrap()
  inspect(outputs.length(), content="1")
  inspect(outputs[0].path().unwrap(), content=output_path)
  let written = @stream.file_byte_stream(output_path, chunk_size=16).unwrap()
  inspect(written.read_all(max_bytes=16).unwrap(), content="hello")
}

///|
test "dest accepts through2 vinyl transformer flush output" {
  let out = "/tmp/mulp-vinyl-fs-through2-flush"
  let input_path = out + "/app.txt"
  let end_path = out + "/end.txt"
  ignore(remove_path_for_test(input_path))
  ignore(remove_path_for_test(end_path))
  let input = @vinyl.vinyl_file(
    cwd="/workspace",
    base="/workspace/src",
    path=Some("/workspace/src/app.txt"),
    contents=@vinyl.Buffer(@utf8.encode("hello")),
  )
  let make = @through2.vinyl_transformer(
    fn(file : @vinyl.VinylFile) -> Result[@vinyl.VinylFile?, @core.MulpError] {
      Ok(Some(file))
    },
    flush=Some(fn() -> Result[Array[@vinyl.VinylFile], @core.MulpError] {
      Ok([
        @vinyl.vinyl_file(
          cwd="/workspace",
          base="/workspace/src",
          path=Some("/workspace/src/end.txt"),
          contents=@vinyl.Buffer(@utf8.encode("end")),
        ),
      ])
    }),
  )
  let outputs = vinyl_file_stream([input])
    .pipe_stream(make())
    .pipe(dest(out))
    .collect(dest_test_context())
    .unwrap()
  inspect(outputs.length(), content="2")
  inspect(outputs[0].path().unwrap(), content=input_path)
  inspect(outputs[1].path().unwrap(), content=end_path)
  let written = @stream.file_byte_stream(end_path, chunk_size=16).unwrap()
  inspect(written.read_all(max_bytes=16).unwrap(), content="end")
}

///|
test "dest writes directories to the output directory" {
  let output_path = "/tmp/mulp-vinyl-fs-dest-directory/nested"
  ignore(remove_path_for_test(output_path))
  let input = @vinyl.vinyl_file(
    cwd="/workspace",
    base="/workspace/src",
    path=Some("/workspace/src/nested"),
    contents=@vinyl.Null,
    stat=Some(@vinyl.vinyl_stat(kind=@vinyl.Directory)),
  )
  let outputs = vinyl_file_stream([input])
    .pipe(dest("/tmp/mulp-vinyl-fs-dest-directory"))
    .collect(dest_test_context())
    .unwrap()
  inspect(outputs.length(), content="1")
  inspect(outputs[0].path().unwrap(), content=output_path)
  inspect(outputs[0].is_directory(), content="true")
  inspect(path_exists_for_test(output_path), content="true")
}

///|
test "dest writes streaming vinyl contents" {
  let output_path = "/tmp/mulp-vinyl-fs-dest-stream/stream.txt"
  ignore(remove_path_for_test(output_path))
  let input = @vinyl.vinyl_file(
    cwd="/workspace",
    base="/workspace/src",
    path=Some("/workspace/src/stream.txt"),
    contents=@vinyl.Stream(@stream.byte_stream(["streamed"])),
  )
  let outputs = vinyl_file_stream([input])
    .pipe(dest("/tmp/mulp-vinyl-fs-dest-stream"))
    .collect(dest_test_context())
    .unwrap()
  inspect(outputs.length(), content="1")
  let written = @stream.file_byte_stream(output_path, chunk_size=16).unwrap()
  inspect(written.read_all(max_bytes=16).unwrap(), content="streamed")
}

///|
test "dest writes symbolic vinyl files as symlinks" {
  if dest_symlink_supported_for_test() {
    write_text_fixture(
      "/tmp/mulp-vinyl-fs-dest-symlink-source", "target.txt", "target",
    )
    let input = @vinyl.vinyl_file(
      cwd="/workspace",
      base="/workspace/src",
      path=Some("/workspace/src/link.txt"),
      contents=@vinyl.Null,
      stat=Some(@vinyl.vinyl_stat(kind=@vinyl.SymbolicLink)),
      symlink=Some("/tmp/mulp-vinyl-fs-dest-symlink-source/target.txt"),
    )
    let outputs = vinyl_file_stream([input])
      .pipe(dest("/tmp/mulp-vinyl-fs-dest-symlink-out"))
      .collect(dest_test_context())
      .unwrap()
    inspect(outputs[0].is_symbolic(), content="true")
    let linked = @stream.file_byte_stream(
      "/tmp/mulp-vinyl-fs-dest-symlink-out/link.txt",
      chunk_size=16,
    ).unwrap()
    inspect(linked.read_all(max_bytes=16).unwrap(), content="target")
  } else {
    inspect(dest_symlink_supported_for_test(), content="false")
  }
}

///|
#cfg(not(platform="windows"))
test "dest preserves existing symlink when overwrite is false" {
  if dest_symlink_supported_for_test() {
    let source = "/tmp/mulp-vinyl-fs-dest-symlink-overwrite-source"
    let out = "/tmp/mulp-vinyl-fs-dest-symlink-overwrite-out"
    write_text_fixture(source, "first.txt", "first")
    write_text_fixture(source, "second.txt", "second")
    let existing = @vinyl.vinyl_file(
      cwd="/workspace",
      base="/workspace/src",
      path=Some("/workspace/src/link.txt"),
      contents=@vinyl.Null,
      stat=Some(@vinyl.vinyl_stat(kind=@vinyl.SymbolicLink)),
      symlink=Some(source + "/first.txt"),
    )
    ignore(
      vinyl_file_stream([existing]).pipe(dest(out)).collect(dest_test_context()),
    )
    let replacement = @vinyl.vinyl_file(
      cwd="/workspace",
      base="/workspace/src",
      path=Some("/workspace/src/link.txt"),
      contents=@vinyl.Null,
      stat=Some(@vinyl.vinyl_stat(kind=@vinyl.SymbolicLink)),
      symlink=Some(source + "/second.txt"),
    )
    ignore(
      vinyl_file_stream([replacement])
      .pipe(dest_with_options(out, dest_options(overwrite=false)))
      .collect(dest_test_context()),
    )
    let linked = @stream.file_byte_stream(out + "/link.txt", chunk_size=16).unwrap()
    inspect(linked.read_all(max_bytes=16).unwrap(), content="first")
  } else {
    inspect(dest_symlink_supported_for_test(), content="false")
  }
}

///|
test "dest errors when symbolic vinyl files do not have symlink targets" {
  let input = @vinyl.vinyl_file(
    cwd="/workspace",
    base="/workspace/src",
    path=Some("/workspace/src/link.txt"),
    contents=@vinyl.Null,
    stat=Some(@vinyl.vinyl_stat(kind=@vinyl.SymbolicLink)),
  )
  let result = vinyl_file_stream([input])
    .pipe(dest("/tmp/mulp-vinyl-fs-dest-missing-symlink"))
    .collect(dest_test_context())
  inspect(result is Err(_), content="true")
}

///|
test "dest resolves relative output directory from cwd option" {
  let input = @vinyl.vinyl_file(
    cwd="/workspace",
    base="/workspace/src",
    path=Some("/workspace/src/main.txt"),
    contents=@vinyl.Buffer(@utf8.encode("cwd")),
  )
  let options = dest_options(cwd="/tmp")
  let outputs = vinyl_file_stream([input])
    .pipe(dest_with_options("mulp-vinyl-fs-dest-cwd", options))
    .collect(dest_test_context())
    .unwrap()
  inspect(
    outputs[0].path().unwrap(),
    content="/tmp/mulp-vinyl-fs-dest-cwd/main.txt",
  )
}

///|
test "dest applies mode and dirMode to returned vinyl stats" {
  let file_input = @vinyl.vinyl_file(
    cwd="/workspace",
    base="/workspace/src",
    path=Some("/workspace/src/app.txt"),
    contents=@vinyl.Buffer(@utf8.encode("mode")),
  )
  let dir_input = @vinyl.vinyl_file(
    cwd="/workspace",
    base="/workspace/src",
    path=Some("/workspace/src/assets"),
    contents=@vinyl.Null,
    stat=Some(@vinyl.vinyl_stat(kind=@vinyl.Directory)),
  )
  let options = dest_options(mode=Some(0o600), dir_mode=Some(0o755))
  let outputs = vinyl_file_stream([file_input, dir_input])
    .pipe(dest_with_options("/tmp/mulp-vinyl-fs-dest-modes", options))
    .collect(dest_test_context())
    .unwrap()
  inspect(outputs[0].stat().unwrap().mode().unwrap(), content="384")
  inspect(outputs[1].stat().unwrap().mode().unwrap(), content="493")
  inspect(outputs[0].metadata_value("mode").unwrap(), content="384")
  inspect(outputs[1].metadata_value("mode").unwrap(), content="493")
}

///|
#cfg(not(platform="windows"))
test "dest applies mode and dirMode to filesystem entries" {
  let out = "/tmp/mulp-vinyl-fs-dest-real-modes"
  let file_path = out + "/app.txt"
  let dir_path = out + "/assets"
  ignore(remove_path_for_test(file_path))
  let file_input = @vinyl.vinyl_file(
    cwd="/workspace",
    base="/workspace/src",
    path=Some("/workspace/src/app.txt"),
    contents=@vinyl.Buffer(@utf8.encode("mode")),
  )
  let dir_input = @vinyl.vinyl_file(
    cwd="/workspace",
    base="/workspace/src",
    path=Some("/workspace/src/assets"),
    contents=@vinyl.Null,
    stat=Some(@vinyl.vinyl_stat(kind=@vinyl.Directory)),
  )
  let options = dest_options(mode=Some(0o600), dir_mode=Some(0o755))
  ignore(
    vinyl_file_stream([file_input, dir_input])
    .pipe(dest_with_options(out, options))
    .collect(dest_test_context()),
  )
  inspect(file_metadata(file_path).unwrap().mode().unwrap(), content="600")
  inspect(file_metadata(dir_path).unwrap().mode().unwrap(), content="755")
}

///|
#cfg(not(platform="windows"))
test "dest applies mtime to filesystem files" {
  let out = "/tmp/mulp-vinyl-fs-dest-real-times"
  let file_path = out + "/app.txt"
  ignore(remove_path_for_test(file_path))
  let input = @vinyl.vinyl_file(
    cwd="/workspace",
    base="/workspace/src",
    path=Some("/workspace/src/app.txt"),
    contents=@vinyl.Buffer(@utf8.encode("time")),
    stat=Some(@vinyl.vinyl_stat(kind=@vinyl.File, mtime_ms=Some(123456000L))),
  )
  ignore(
    vinyl_file_stream([input]).pipe(dest(out)).collect(dest_test_context()),
  )
  inspect(
    file_metadata(file_path).unwrap().mtime_ms().unwrap(),
    content="123456000",
  )
}

///|
test "dest preserves existing file when overwrite is false" {
  let out = "/tmp/mulp-vinyl-fs-dest-no-overwrite"
  let output_path = out + "/main.txt"
  ignore(remove_path_for_test(output_path))
  write_text_fixture(out, "main.txt", "existing")
  let input = @vinyl.vinyl_file(
    cwd="/workspace",
    base="/workspace/src",
    path=Some("/workspace/src/main.txt"),
    contents=@vinyl.Buffer(@utf8.encode("new")),
  )
  let outputs = vinyl_file_stream([input])
    .pipe(dest_with_options(out, dest_options(overwrite=false)))
    .collect(dest_test_context())
    .unwrap()
  inspect(outputs.length(), content="1")
  let written = @stream.file_byte_stream(output_path, chunk_size=16).unwrap()
  inspect(written.read_all(max_bytes=16).unwrap(), content="existing")
}

///|
test "dest appends file contents when append is true" {
  let out = "/tmp/mulp-vinyl-fs-dest-append"
  let output_path = out + "/main.txt"
  ignore(remove_path_for_test(output_path))
  write_text_fixture(out, "main.txt", "hello")
  let input = @vinyl.vinyl_file(
    cwd="/workspace",
    base="/workspace/src",
    path=Some("/workspace/src/main.txt"),
    contents=@vinyl.Buffer(@utf8.encode(" world")),
  )
  let outputs = vinyl_file_stream([input])
    .pipe(dest_with_options(out, dest_options(append=true)))
    .collect(dest_test_context())
    .unwrap()
  inspect(outputs.length(), content="1")
  let written = @stream.file_byte_stream(output_path, chunk_size=32).unwrap()
  inspect(written.read_all(max_bytes=32).unwrap(), content="hello world")
}

///|
test "dest writes external sourcemap files when sourcemaps location is set" {
  let out = "/tmp/mulp-vinyl-fs-dest-sourcemaps"
  let output_path = out + "/main.js"
  let map_path = out + "/main.js.map"
  ignore(remove_path_for_test(output_path))
  ignore(remove_path_for_test(map_path))
  let input = @vinyl.vinyl_file(
    cwd="/workspace",
    base="/workspace/src",
    path=Some("/workspace/src/main.js"),
    contents=@vinyl.Buffer(@utf8.encode("console.log(1)")),
    source_map=Some("{\"version\":3}"),
  )
  let outputs = vinyl_file_stream([input])
    .pipe(dest_with_options(out, dest_options(sourcemaps=Some("."))))
    .collect(dest_test_context())
    .unwrap()
  inspect(outputs.length(), content="2")
  inspect(outputs[0].path().unwrap(), content=output_path)
  inspect(outputs[1].path().unwrap(), content=map_path)
  let map = @stream.file_byte_stream(map_path, chunk_size=32).unwrap()
  inspect(map.read_all(max_bytes=32).unwrap(), content="{\"version\":3}")
}

///|
test "dest inlines sourcemap files when inline_sourcemaps is true" {
  let out = "/tmp/mulp-vinyl-fs-dest-inline-sourcemaps"
  let output_path = out + "/main.js"
  let map_path = out + "/main.js.map"
  ignore(remove_path_for_test(output_path))
  ignore(remove_path_for_test(map_path))
  let input = @vinyl.vinyl_file(
    cwd="/workspace",
    base="/workspace/src",
    path=Some("/workspace/src/main.js"),
    contents=@vinyl.Buffer(@utf8.encode("console.log(1)")),
    source_map=Some("{\"version\":3}"),
  )
  let outputs = vinyl_file_stream([input])
    .pipe(dest_with_options(out, dest_options(inline_sourcemaps=true)))
    .collect(dest_test_context())
    .unwrap()
  inspect(outputs.length(), content="1")
  inspect(path_exists_for_test(map_path), content="false")
  match outputs[0].contents() {
    @vinyl.Buffer(bytes) =>
      inspect(
        @utf8.decode_lossy(bytes).contains(
          "sourceMappingURL=data:application/json;charset=utf-8;base64,eyJ2ZXJzaW9uIjozfQ==",
        ),
        content="true",
      )
    _ => inspect("wrong contents", content="Buffer")
  }
  let written = @stream.file_byte_stream(output_path, chunk_size=256).unwrap()
  inspect(
    written
    .read_all(max_bytes=256)
    .unwrap()
    .contains(
      "sourceMappingURL=data:application/json;charset=utf-8;base64,eyJ2ZXJzaW9uIjozfQ==",
    ),
    content="true",
  )
}

///|
test "dest skips files read with read false" {
  let root = "/tmp/mulp-vinyl-fs-read-false-src"
  let out = "/tmp/mulp-vinyl-fs-read-false-out"
  let output_path = out + "/skip.txt"
  ignore(remove_path_for_test(output_path))
  write_text_fixture(root, "skip.txt", "skip")
  let outputs = src(["skip.txt"], src_options(cwd=root, read=false))
    .pipe(dest(out))
    .collect(dest_test_context())
    .unwrap()
  inspect(outputs.length(), content="1")
  inspect(outputs[0].path().unwrap(), content=output_path)
  inspect(outputs[0].is_null(), content="true")
  inspect(path_exists_for_test(output_path), content="false")
}

///|
#borrow(path)
extern "c" fn remove_path_for_test_ffi(path : Bytes) -> Bool = "mulp_vinyl_fs_remove_path_for_test"

///|
#borrow(path)
extern "c" fn path_exists_for_test_ffi(path : Bytes) -> Bool = "mulp_vinyl_fs_path_exists_for_test"

///|
fn remove_path_for_test(path : String) -> Bool {
  remove_path_for_test_ffi(@utf8.encode(path))
}

///|
fn path_exists_for_test(path : String) -> Bool {
  path_exists_for_test_ffi(@utf8.encode(path))
}

///|
extern "c" fn dest_symlink_supported_for_test() -> Bool = "mulp_symlink_supported_for_test"
