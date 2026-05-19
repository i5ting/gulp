///|
fn sample_file(name : String) -> File {
  file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/" + name,
    contents=Empty,
  )
}

///|
test "file stream yields files one at a time" {
  let stream = file_stream([sample_file("a.mbt"), sample_file("b.mbt")])
  inspect(stream.next().unwrap().unwrap().relative_path(), content="a.mbt")
  inspect(stream.next().unwrap().unwrap().relative_path(), content="b.mbt")
  inspect(stream.next().unwrap() is None, content="true")
}

///|
test "collect gathers files when context is active" {
  let stream = file_stream([sample_file("a.mbt"), sample_file("b.mbt")])
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let files = stream.collect(ctx).unwrap()
  inspect(files.length(), content="2")
}

///|
test "collect stops when context is cancelled" {
  let token = @core.new_cancellation_token()
  token.cancel()
  let stream = file_stream([sample_file("a.mbt")])
  let ctx = @core.new_context(cwd="/workspace", now_ms=0L, cancellation=token)
  let result = stream.collect(ctx)
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Task failed: stream: cancelled",
  )
}

///|
test "collect closes upstream when context is cancelled mid stream" {
  let token = @core.new_cancellation_token()
  let ctx = @core.new_context(cwd="/workspace", now_ms=0L, cancellation=token)
  let events : Array[String] = []
  let mut emitted = false
  let stream = file_stream_from_pull_with_close(
    fn() -> Result[File?, @core.MulpError] {
      if emitted {
        Ok(None)
      } else {
        emitted = true
        token.cancel()
        Ok(Some(sample_file("a.mbt")))
      }
    },
    fn() -> Unit { events.push("closed") },
  )
  let result = stream.collect(ctx)
  inspect(result is Err(_), content="true")
  inspect(events, content="[\"closed\"]")
}

///|
test "pipe filters files with zero output" {
  let stream = file_stream([sample_file("a.mbt"), sample_file("skip.tmp")])
  let filtered = stream.pipe(fn(file : File) -> FileStream {
    if file.path.has_suffix(".tmp") {
      file_stream([])
    } else {
      file_stream([file])
    }
  })
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let files = filtered.collect(ctx).unwrap()
  inspect(files.length(), content="1")
  inspect(files[0].relative_path(), content="a.mbt")
}

///|
test "pipe expands one input to many outputs" {
  let stream = file_stream([sample_file("a.mbt")])
  let expanded = stream.pipe(fn(source : File) -> FileStream {
    let copy = file(
      cwd=source.cwd,
      base=source.base,
      path=source.path + ".copy",
      contents=Empty,
    )
    file_stream([source, copy])
  })
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let files = expanded.collect(ctx).unwrap()
  inspect(files.length(), content="2")
  inspect(files[1].relative_path(), content="a.mbt.copy")
}

///|
test "pipe pulls upstream one file at a time" {
  let pulls : Array[String] = []
  let files = [sample_file("a.mbt"), sample_file("b.mbt")]
  let mut index = 0
  let stream = file_stream_from_pull(fn() -> Result[File?, @core.MulpError] {
    pulls.push("pull:\{index}")
    if index >= files.length() {
      Ok(None)
    } else {
      let file = files[index]
      index = index + 1
      Ok(Some(file))
    }
  }).pipe(fn(file : File) -> FileStream {
    file_stream([file])
  })
  inspect(pulls, content="[]")
  inspect(stream.next().unwrap().unwrap().relative_path(), content="a.mbt")
  inspect(pulls, content="[\"pull:0\"]")
  inspect(stream.next().unwrap().unwrap().relative_path(), content="b.mbt")
  inspect(pulls, content="[\"pull:0\", \"pull:1\"]")
}

///|
test "pipe drains current transform before reading next upstream file" {
  let pulls : Array[String] = []
  let files = [sample_file("a.mbt"), sample_file("b.mbt")]
  let mut index = 0
  let stream = file_stream_from_pull(fn() -> Result[File?, @core.MulpError] {
    pulls.push("pull:\{index}")
    if index >= files.length() {
      Ok(None)
    } else {
      let file = files[index]
      index = index + 1
      Ok(Some(file))
    }
  }).pipe(fn(source : File) -> FileStream {
    file_stream([
      source,
      file(
        cwd=source.cwd,
        base=source.base,
        path=source.path + ".copy",
        contents=Empty,
      ),
    ])
  })
  inspect(stream.next().unwrap().unwrap().relative_path(), content="a.mbt")
  inspect(pulls, content="[\"pull:0\"]")
  inspect(stream.next().unwrap().unwrap().relative_path(), content="a.mbt.copy")
  inspect(pulls, content="[\"pull:0\"]")
  inspect(stream.next().unwrap().unwrap().relative_path(), content="b.mbt")
  inspect(pulls, content="[\"pull:0\", \"pull:1\"]")
}

///|
test "closing piped stream closes active transform and upstream" {
  let events : Array[String] = []
  let mut upstream_index = 0
  let upstream = file_stream_from_pull_with_close(
    fn() -> Result[File?, @core.MulpError] {
      if upstream_index > 0 {
        Ok(None)
      } else {
        upstream_index = upstream_index + 1
        Ok(Some(sample_file("a.mbt")))
      }
    },
    fn() -> Unit { events.push("upstream closed") },
  )
  let stream = upstream.pipe(fn(file : File) -> FileStream {
    let mut emitted = false
    file_stream_from_pull_with_close(
      fn() -> Result[File?, @core.MulpError] {
        if emitted {
          Ok(None)
        } else {
          emitted = true
          Ok(Some(file))
        }
      },
      fn() -> Unit { events.push("transform closed") },
    )
  })
  inspect(stream.next().unwrap().unwrap().relative_path(), content="a.mbt")
  inspect(events, content="[]")
  stream.close()
  inspect(events, content="[\"transform closed\", \"upstream closed\"]")
  inspect(stream.next().unwrap() is None, content="true")
}

///|
test "closing merged stream closes every source" {
  let events : Array[String] = []
  let mut first_emitted = false
  let first = file_stream_from_pull_with_close(
    fn() -> Result[File?, @core.MulpError] {
      if first_emitted {
        Ok(None)
      } else {
        first_emitted = true
        Ok(Some(sample_file("a.mbt")))
      }
    },
    fn() -> Unit { events.push("first closed") },
  )
  let second = file_stream_from_pull_with_close(
    fn() -> Result[File?, @core.MulpError] { Ok(Some(sample_file("b.mbt"))) },
    fn() -> Unit { events.push("second closed") },
  )
  let merged = merge_sources([first, second])
  inspect(merged.next().unwrap().unwrap().relative_path(), content="a.mbt")
  inspect(events, content="[]")
  merged.close()
  inspect(events, content="[\"first closed\", \"second closed\"]")
}

///|
test "closing plugin stream closes active inner transform and upstream" {
  let events : Array[String] = []
  let upstream = file_stream_from_pull_with_close(
    fn() -> Result[File?, @core.MulpError] { Ok(Some(sample_file("a.mbt"))) },
    fn() -> Unit { events.push("upstream closed") },
  )
  let stream = upstream.pipe_plugin(plugin("closable", fn(file : File) -> FileStream {
    let mut emitted = false
    file_stream_from_pull_with_close(
      fn() -> Result[File?, @core.MulpError] {
        if emitted {
          Ok(None)
        } else {
          emitted = true
          Ok(Some(file))
        }
      },
      fn() -> Unit { events.push("inner closed") },
    )
  }))
  inspect(stream.next().unwrap().unwrap().relative_path(), content="a.mbt")
  stream.close()
  inspect(events, content="[\"inner closed\", \"upstream closed\"]")
}

///|
test "pipe propagates upstream errors" {
  let stream = file_stream_from_pull(fn() -> Result[File?, @core.MulpError] {
    Err(@core.stream_error("upstream failed"))
  })
  let piped = stream.pipe(fn(file : File) -> FileStream { file_stream([file]) })
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let result = piped.collect(ctx)
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Stream error: upstream failed",
  )
}

///|
test "pipe propagates transform stream errors" {
  let stream = file_stream([sample_file("a.mbt")])
  let piped = stream.pipe(fn(_file : File) -> FileStream {
    file_stream_from_pull(fn() -> Result[File?, @core.MulpError] {
      Err(@core.stream_error("transform failed"))
    })
  })
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let result = piped.collect(ctx)
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Stream error: transform failed",
  )
}

///|
test "pipe stops pulling upstream after transform error" {
  let pulls : Array[String] = []
  let files = [sample_file("a.mbt"), sample_file("b.mbt")]
  let mut index = 0
  let stream = file_stream_from_pull(fn() -> Result[File?, @core.MulpError] {
    pulls.push("pull:\{index}")
    if index >= files.length() {
      Ok(None)
    } else {
      let file = files[index]
      index = index + 1
      Ok(Some(file))
    }
  }).pipe(fn(_file : File) -> FileStream {
    error_stream(@core.stream_error("transform failed"))
  })
  let result = stream.next()
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Stream error: transform failed",
  )
  inspect(pulls, content="[\"pull:0\"]")
  inspect(stream.next().unwrap() is None, content="true")
}

///|
test "stream plugin wraps a named transform" {
  let rename = plugin("rename-copy", fn(source : File) -> FileStream {
    let copy = file(
      cwd=source.cwd,
      base=source.base,
      path=source.path + ".copy",
      contents=source.contents,
    )
    file_stream([copy])
  })
  inspect(rename.name(), content="rename-copy")
  let stream = file_stream([sample_file("a.mbt")]).pipe_plugin(rename)
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let files = stream.collect(ctx).unwrap()
  inspect(files.length(), content="1")
  inspect(files[0].relative_path(), content="a.mbt.copy")
}

///|
test "stream plugin can carry string config values" {
  let configured = plugin_with_config(
    "rename",
    [("suffix", ".min"), ("mode", "strict")],
    fn(source : File) -> FileStream { file_stream([source]) },
  )
  inspect(configured.name(), content="rename")
  inspect(configured.config_value("suffix").unwrap(), content=".min")
  inspect(configured.config_value("mode").unwrap(), content="strict")
  inspect(configured.config_value("missing") is None, content="true")
}

///|
test "plugin package names follow mulp export convention" {
  inspect(plugin_package_name_is_valid("mulp-rename"), content="true")
  inspect(plugin_package_name_is_valid("@acme/mulp-filter"), content="true")
  inspect(plugin_package_name_is_valid("gulp-rename"), content="false")
  inspect(plugin_export_name("mulp-rename"), content="rename")
  inspect(plugin_export_name("@acme/mulp-filter"), content="filter")
}

///|
test "rename plugin rewrites file path" {
  let stream = file_stream([sample_file("a.mbt")]).pipe_plugin(
    rename_plugin(fn(file : File) -> String {
      file.dirname() + "/renamed" + file.extname()
    }),
  )
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let files = stream.collect(ctx).unwrap()
  inspect(files.length(), content="1")
  inspect(files[0].path, content="/workspace/src/renamed.mbt")
  inspect(files[0].relative_path(), content="renamed.mbt")
}

///|
test "filter plugin keeps matching files" {
  let stream = file_stream([sample_file("a.mbt"), sample_file("a.txt")]).pipe_plugin(
    filter_plugin(fn(file : File) -> Bool { file.extname() == ".mbt" }),
  )
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let files = stream.collect(ctx).unwrap()
  inspect(files.length(), content="1")
  inspect(files[0].relative_path(), content="a.mbt")
}

///|
test "concat plugin emits one buffered file" {
  let stream = file_stream([
    file(
      cwd="/workspace",
      base="/workspace/src",
      path="/workspace/src/a.txt",
      contents=Text("hello"),
    ),
    file(
      cwd="/workspace",
      base="/workspace/src",
      path="/workspace/src/b.txt",
      contents=Text(" world"),
    ),
  ]).pipe_stream(concat_plugin("bundle.txt"))
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let files = stream.collect(ctx).unwrap()
  inspect(files.length(), content="1")
  inspect(files[0].relative_path(), content="bundle.txt")
  match files[0].contents {
    Text(text) => inspect(text, content="hello world")
    _ => inspect("wrong contents", content="Buffer")
  }
}

///|
test "concat plugin rejects non buffered files" {
  let stream = file_stream([
    file(
      cwd="/workspace",
      base="/workspace/src",
      path="/workspace/src/assets",
      contents=Directory,
    ),
  ]).pipe_stream(concat_plugin("bundle.txt"))
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let result = stream.collect(ctx)
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Stream error: concat only supports buffered files",
  )
}

///|
test "plugin test helper collects plugin output" {
  let files = test_plugin(
    rename_plugin(fn(file : File) -> String { file.path + ".out" }),
    [sample_file("a.mbt")],
  ).unwrap()
  inspect(files.length(), content="1")
  inspect(files[0].relative_path(), content="a.mbt.out")
}

///|
test "stream plugin can propagate an explicit error stream" {
  let fail = plugin("fail", fn(_source : File) -> FileStream {
    error_stream(@core.stream_error("plugin failed"))
  })
  let stream = file_stream([sample_file("a.mbt")]).pipe_plugin(fail)
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let result = stream.collect(ctx)
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Stream error: plugin fail: Stream error: plugin failed",
  )
}

///|
test "merge sources yields files from multiple streams in order" {
  let merged = merge_sources([
    file_stream([sample_file("a.mbt")]),
    file_stream([sample_file("b.mbt"), sample_file("c.mbt")]),
  ])
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let files = merged.collect(ctx).unwrap()
  inspect(files.length(), content="3")
  inspect(files[0].relative_path(), content="a.mbt")
  inspect(files[2].relative_path(), content="c.mbt")
}

///|
test "filter map and flat map helpers build common transforms" {
  let stream = file_stream([sample_file("a.mbt"), sample_file("skip.tmp")])
    .pipe(
      filter_files(fn(file : File) -> Bool { file.path.has_suffix(".mbt") }),
    )
    .pipe(
      map_files(fn(source : File) -> File {
        file(
          cwd="/workspace",
          base="/workspace/src",
          path=source.path + ".mapped",
          contents=Empty,
        )
      }),
    )
    .pipe(
      flat_map_files(fn(file : File) -> Array[File] {
        [file, sample_file("extra.mbt")]
      }),
    )
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let files = stream.collect(ctx).unwrap()
  inspect(files.length(), content="2")
  inspect(files[0].relative_path(), content="a.mbt.mapped")
  inspect(files[1].relative_path(), content="extra.mbt")
}
