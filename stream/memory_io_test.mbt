///|
fn memory_file(path : String, chunks : Array[String]) -> File {
  file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/" + path,
    contents=Bytes(byte_stream(chunks)),
  )
}

///|
test "src emits in-memory files lazily" {
  let stream = src([memory_file("a.txt", ["a"]), memory_file("b.txt", ["b"])])
  inspect(stream.next().unwrap().unwrap().relative_path(), content="a.txt")
  inspect(stream.next().unwrap().unwrap().relative_path(), content="b.txt")
  inspect(stream.next().unwrap() is None, content="true")
}

///|
test "src paths resolves relative paths from cwd" {
  let stream = src_paths(
    ["src/a.txt", "/workspace/src/b.txt"],
    cwd="/workspace",
    base="/workspace/src",
  )
  let first = stream.next().unwrap().unwrap()
  let second = stream.next().unwrap().unwrap()
  inspect(first.path, content="/workspace/src/a.txt")
  inspect(first.cwd, content="/workspace")
  inspect(first.base, content="/workspace/src")
  inspect(first.relative_path(), content="a.txt")
  inspect(second.path, content="/workspace/src/b.txt")
  inspect(second.relative_path(), content="b.txt")
}

///|
test "src paths uses base for relative paths" {
  let stream = src_paths(["src/app/a.txt"], cwd="/workspace", base="/workspace")
  let file = stream.next().unwrap().unwrap()
  inspect(file.path, content="/workspace/src/app/a.txt")
  inspect(file.base, content="/workspace")
  inspect(file.relative_path(), content="src/app/a.txt")
}

///|
test "src paths supports read false empty contents" {
  let stream = src_paths_with_options(
    ["src/app/a.txt"],
    cwd="/workspace",
    base="/workspace/src",
    read=false,
    buffer=true,
  )
  let file = stream.next().unwrap().unwrap()
  match file.contents {
    Empty => inspect(file.relative_path(), content="app/a.txt")
    _ => inspect("wrong contents", content="empty")
  }
}

///|
test "src paths supports buffer false stream contents" {
  let stream = src_paths_with_options(
    ["src/app/a.txt"],
    cwd="/workspace",
    base="/workspace/src",
    read=true,
    buffer=false,
  )
  let file = stream.next().unwrap().unwrap()
  match file.contents {
    Bytes(bytes) => {
      inspect(file.relative_path(), content="app/a.txt")
      inspect(bytes.read_next().unwrap() is None, content="true")
    }
    _ => inspect("wrong contents", content="bytes")
  }
}

///|
test "src since filters files newer than last run" {
  let older = file_with_metadata(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/older.txt",
    contents=Text("old"),
    metadata=[("mtime_ms", "1000")],
  )
  let newer = file_with_metadata(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/newer.txt",
    contents=Text("new"),
    metadata=[("mtime_ms", "3000")],
  )
  let stream = src_since([older, newer], since=Some(2000L))
  let file = stream.next().unwrap().unwrap()
  inspect(file.relative_path(), content="newer.txt")
  inspect(stream.next().unwrap() is None, content="true")
}

///|
test "src since accepts last run timestamps" {
  let task = @core.new_task("build", fn(
    _ctx : @core.Context,
  ) -> Result[Unit, @core.MulpError] {
    Ok(())
  })
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=2000L,
    cancellation=@core.new_cancellation_token(),
  )
  ignore(task.execute(ctx))
  let changed = file_with_metadata(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/changed.txt",
    contents=Text("new"),
    metadata=[("mtime_ms", "2001")],
  )
  let stream = src_since([changed], since=@core.last_run(task))
  inspect(
    stream.next().unwrap().unwrap().relative_path(),
    content="changed.txt",
  )
}

///|
test "src symlink options can preserve or follow links" {
  let target = file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/target.txt",
    contents=Text("target"),
  )
  let link = file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/link.txt",
    contents=Symlink("/workspace/src/target.txt"),
  )
  let no_follow = src_symlinks([link, target], follow=false)
  match no_follow.next().unwrap().unwrap().contents {
    Symlink(target) => inspect(target, content="/workspace/src/target.txt")
    _ => inspect("wrong contents", content="symlink")
  }
  let followed = src_symlinks([link, target], follow=true)
  let file = followed.next().unwrap().unwrap()
  inspect(file.path, content="/workspace/src/link.txt")
  match file.contents {
    Text(contents) => inspect(contents, content="target")
    _ => inspect("wrong contents", content="buffer")
  }
}

///|
test "dest writes files under output directory preserving relative path" {
  let sink = memory_dest()
  let pipeline = src([memory_file("nested/a.txt", ["hello"])]).pipe(
    dest("dist", sink),
  )
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let files = pipeline.collect(ctx).unwrap()
  inspect(files.length(), content="1")
  inspect(files[0].path, content="dist/nested/a.txt")
  inspect(sink.files().length(), content="1")
  inspect(sink.files()[0].path, content="dist/nested/a.txt")
}

///|
test "dest does not duplicate writes on repeated pulls" {
  let sink = memory_dest()
  let stream = src([memory_file("a.txt", ["hello"])]).pipe(dest("dist", sink))
  inspect(stream.next().unwrap().unwrap().path, content="dist/a.txt")
  inspect(stream.next().unwrap() is None, content="true")
  inspect(stream.next().unwrap() is None, content="true")
  inspect(sink.files().length(), content="1")
  inspect(sink.contents_for("dist/a.txt").unwrap(), content="hello")
}

///|
test "dest accepts output directory with trailing slash" {
  let sink = memory_dest()
  let pipeline = src([memory_file("nested/a.txt", ["hello"])]).pipe(
    dest("dist/", sink),
  )
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let files = pipeline.collect(ctx).unwrap()
  inspect(files[0].path, content="dist/nested/a.txt")
  inspect(sink.contents_for("dist/nested/a.txt").unwrap(), content="hello")
}

///|
test "dest consumes byte contents" {
  let sink = memory_dest()
  let pipeline = src([memory_file("a.txt", ["he", "llo"])]).pipe(
    dest("dist", sink),
  )
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  ignore(pipeline.collect(ctx))
  inspect(sink.contents_for("dist/a.txt").unwrap(), content="hello")
}

///|
test "dest consumes buffer contents" {
  let input = file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/a.txt",
    contents=Buffer(Bytes::from_array([b'h', b'e', b'l', b'l', b'o'])),
  )
  let sink = memory_dest()
  let pipeline = src([input]).pipe(dest("dist", sink))
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  ignore(pipeline.collect(ctx))
  inspect(sink.contents_for("dist/a.txt").unwrap(), content="hello")
  inspect(sink.contents_bytes_for("dist/a.txt").unwrap().length(), content="5")
}

///|
test "dest preserves source maps on output files" {
  let input = file_with_source_map(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/a.js",
    contents=Text("console.log(1);"),
    source_map=Some("{\"version\":3}"),
  )
  let sink = memory_dest()
  let pipeline = src([input]).pipe(dest("dist", sink))
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let files = pipeline.collect(ctx).unwrap()
  inspect(files[0].path, content="dist/a.js")
  inspect(files[0].source_map().unwrap(), content="{\"version\":3}")
}

///|
test "dest can emit sourcemap files" {
  let input = file_with_source_map(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/a.js",
    contents=Text("console.log(1);"),
    source_map=Some("{\"version\":3}"),
  )
  let sink = memory_dest()
  let pipeline = src([input]).pipe(
    dest_with_options("dist", sink, sourcemaps=true),
  )
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  ignore(pipeline.collect(ctx))
  inspect(sink.contents_for("dist/a.js").unwrap(), content="console.log(1);")
  inspect(
    sink.contents_for("dist/a.js.map").unwrap(),
    content="{\"version\":3}",
  )
}

///|
test "dest write mode can overwrite or append existing contents" {
  let first = file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/a.txt",
    contents=Text("first"),
  )
  let second = file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/a.txt",
    contents=Text("second"),
  )
  let overwrite_sink = memory_dest()
  let append_sink = memory_dest()
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  ignore(
    src([first, second])
    .pipe(dest_with_write_mode("dist", overwrite_sink, write_mode=Overwrite))
    .collect(ctx),
  )
  inspect(overwrite_sink.contents_for("dist/a.txt").unwrap(), content="second")
  ignore(
    src([first, second])
    .pipe(dest_with_write_mode("dist", append_sink, write_mode=Append))
    .collect(ctx),
  )
  inspect(
    append_sink.contents_for("dist/a.txt").unwrap(),
    content="firstsecond",
  )
}

///|
test "dest can apply file mode and directory mode metadata" {
  let file_input = file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/a.txt",
    contents=Text("a"),
  )
  let dir_input = file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/assets",
    contents=Directory,
  )
  let sink = memory_dest()
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let files = src([file_input, dir_input])
    .pipe(
      dest_with_modes(
        "dist",
        sink,
        mode=Some("100644"),
        dir_mode=Some("040755"),
      ),
    )
    .collect(ctx)
    .unwrap()
  inspect(files[0].mode().unwrap(), content="100644")
  inspect(files[1].mode().unwrap(), content="040755")
}

///|
test "dest preserves file metadata on output files" {
  let input = file_with_metadata(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/a.txt",
    contents=Text("hello"),
    metadata=[("mode", "100644"), ("mtime_ms", "1700000000000")],
  )
  let sink = memory_dest()
  let pipeline = src([input]).pipe(dest("dist", sink))
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let files = pipeline.collect(ctx).unwrap()
  inspect(files[0].path, content="dist/a.txt")
  inspect(files[0].mode().unwrap(), content="100644")
  inspect(files[0].mtime_ms().unwrap(), content="1700000000000")
}

///|
test "dest preserves directory symlink empty and stream content kinds" {
  let directory = file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/assets",
    contents=Directory,
  )
  let symlink = file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/link",
    contents=Symlink("/workspace/target"),
  )
  let empty = file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/empty.txt",
    contents=Empty,
  )
  let streamed = memory_file("stream.txt", ["a", "b"])
  let sink = memory_dest()
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let files = src([directory, symlink, empty, streamed])
    .pipe(dest("dist", sink))
    .collect(ctx)
    .unwrap()
  match files[0].contents {
    Directory => inspect(files[0].path, content="dist/assets")
    _ => inspect("wrong contents", content="directory")
  }
  match files[1].contents {
    Symlink(target) => inspect(target, content="/workspace/target")
    _ => inspect("wrong contents", content="symlink")
  }
  match files[2].contents {
    Empty => inspect(files[2].path, content="dist/empty.txt")
    _ => inspect("wrong contents", content="empty")
  }
  match files[3].contents {
    Bytes(_) => inspect(files[3].path, content="dist/stream.txt")
    _ => inspect("wrong contents", content="bytes")
  }
  inspect(sink.contents_for("dist/stream.txt").unwrap(), content="ab")
}

///|
test "dest propagates byte stream errors" {
  let bytes = byte_stream(["a"])
  ignore(bytes.read_all(max_bytes=10))
  let bad = file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/a.txt",
    contents=Bytes(bytes),
  )
  let sink = memory_dest()
  let pipeline = src([bad]).pipe(dest("dist", sink))
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let result = pipeline.collect(ctx)
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Stream error: byte stream already consumed",
  )
  inspect(sink.files().length(), content="0")
}
