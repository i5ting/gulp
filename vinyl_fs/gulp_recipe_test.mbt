///|
test "gulp recipe copy rename with through2 vinyl transform" {
  let root = "/tmp/mulp-vinyl-fs-recipe-copy-src"
  let out = "/tmp/mulp-vinyl-fs-recipe-copy-out"
  let output_path = out + "/app.out.txt"
  ignore(remove_path_for_test(output_path))
  write_text_fixture(root, "app.txt", "hello")
  let outputs = src(["app.txt"], src_options(cwd=root))
    .pipe_stream(
      @through2.object_transform_vinyl(
        map=Some(fn(file : @vinyl.VinylFile) -> Result[@vinyl.VinylFile?, @core.MulpError] {
          Ok(Some(file.with_path(root + "/app.out.txt")))
        }),
      ),
    )
    .pipe(dest(out))
    .collect(test_context())
    .unwrap()
  inspect(outputs.length(), content="1")
  inspect(outputs[0].path().unwrap(), content=output_path)
  let written = @stream.file_byte_stream(output_path, chunk_size=16).unwrap()
  inspect(written.read_all(max_bytes=16).unwrap(), content="hello")
}

///|
test "gulp recipe filter duplicate with through2 vinyl flat transform" {
  let root = "/tmp/mulp-vinyl-fs-recipe-flat-src"
  let out = "/tmp/mulp-vinyl-fs-recipe-flat-out"
  let app_path = out + "/app.txt"
  let copy_path = out + "/app.copy.txt"
  let ignored_path = out + "/ignored.log"
  ignore(remove_path_for_test(app_path))
  ignore(remove_path_for_test(copy_path))
  ignore(remove_path_for_test(ignored_path))
  write_text_fixture(root, "app.txt", "hello")
  write_text_fixture(root, "ignored.log", "skip")
  let outputs = src(["*.*"], src_options(cwd=root))
    .pipe_stream(
      @through2.object_transform_vinyl_flat(fn(
        file : @vinyl.VinylFile,
      ) -> Result[Array[@vinyl.VinylFile], @core.MulpError] {
        if file.extname() == ".txt" {
          Ok([file, file.with_path(root + "/app.copy.txt")])
        } else {
          Ok([])
        }
      }),
    )
    .pipe(dest(out))
    .collect(test_context())
    .unwrap()
  inspect(outputs.length(), content="2")
  inspect(path_exists_for_test(app_path), content="true")
  inspect(path_exists_for_test(copy_path), content="true")
  inspect(path_exists_for_test(ignored_path), content="false")
}
