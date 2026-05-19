///|
test "file dest writes byte contents to real filesystem" {
  let input = file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/mulp-dest-output.txt",
    contents=Bytes(byte_stream(["he", "llo"])),
  )
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let outputs = src([input]).pipe(file_dest("/tmp")).collect(ctx).unwrap()
  inspect(outputs.length(), content="1")
  inspect(outputs[0].path, content="/tmp/mulp-dest-output.txt")
  let written = file_byte_stream("/tmp/mulp-dest-output.txt", chunk_size=8).unwrap()
  inspect(written.read_all(max_bytes=16).unwrap(), content="hello")
}

///|
test "file dest writes buffer contents to real filesystem" {
  let input = file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/mulp-buffer-output.txt",
    contents=Text("hello"),
  )
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let outputs = src([input]).pipe(file_dest("/tmp")).collect(ctx).unwrap()
  inspect(outputs.length(), content="1")
  inspect(outputs[0].path, content="/tmp/mulp-buffer-output.txt")
  let written = file_byte_stream("/tmp/mulp-buffer-output.txt", chunk_size=8).unwrap()
  inspect(written.read_all(max_bytes=16).unwrap(), content="hello")
}

///|
test "file dest writes native buffer contents to real filesystem" {
  let input = file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/mulp-native-buffer-output.txt",
    contents=Buffer(Bytes::from_array([b'h', b'e', b'l', b'l', b'o'])),
  )
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let outputs = src([input]).pipe(file_dest("/tmp")).collect(ctx).unwrap()
  inspect(outputs.length(), content="1")
  let written = file_byte_stream(
    "/tmp/mulp-native-buffer-output.txt",
    chunk_size=8,
  ).unwrap()
  inspect(written.read_all(max_bytes=16).unwrap(), content="hello")
}

///|
test "file dest resolves relative output directory from cwd option" {
  let input = file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/cwd-output.txt",
    contents=Text("cwd"),
  )
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let outputs = src([input])
    .pipe(file_dest_with_options("mulp-dest-cwd", cwd=Some("/tmp")))
    .collect(ctx)
    .unwrap()
  inspect(outputs[0].path, content="/tmp/mulp-dest-cwd/cwd-output.txt")
  let written = file_byte_stream(
    "/tmp/mulp-dest-cwd/cwd-output.txt",
    chunk_size=8,
  ).unwrap()
  inspect(written.read_all(max_bytes=8).unwrap(), content="cwd")
}

///|
fn one_megabyte_chunk() -> String {
  let mut chunk = "x"
  for _ in 0..<20 {
    chunk = chunk + chunk
  }
  chunk
}

///|
fn large_byte_stream() -> ByteStream {
  let chunk = @utf8.encode(one_megabyte_chunk())
  let mut remaining = 17
  byte_stream_from_pull(fn() -> Result[Bytes?, @core.MulpError] {
    if remaining == 0 {
      Ok(None)
    } else {
      remaining = remaining - 1
      Ok(Some(chunk))
    }
  })
}

///|
fn dest_metadata_value(
  metadata : Array[(String, String)],
  key : String,
) -> String? {
  for item in metadata {
    if item.0 == key {
      return Some(item.1)
    }
  }
  None
}

///|
test "file dest streams files larger than read all limit" {
  let input = file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/large.txt",
    contents=Bytes(large_byte_stream()),
  )
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let result = src([input]).pipe(file_dest("/tmp/mulp-large-dest")).collect(ctx)
  inspect(result is Ok(_), content="true")
  let metadata = real_file_metadata("/tmp/mulp-large-dest/large.txt").unwrap()
  inspect(dest_metadata_value(metadata, "size").unwrap(), content="17825792")
}

///|
test "file dest creates parent directories" {
  let input = file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/nested/mulp-dest-parent.txt",
    contents=Bytes(byte_stream(["parent"])),
  )
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let result = src([input])
    .pipe(file_dest("/tmp/mulp-dest-parent"))
    .collect(ctx)
  inspect(result is Ok(_), content="true")
  match result {
    Ok(outputs) => {
      inspect(
        outputs[0].path,
        content="/tmp/mulp-dest-parent/nested/mulp-dest-parent.txt",
      )
      let written = file_byte_stream(
        "/tmp/mulp-dest-parent/nested/mulp-dest-parent.txt",
        chunk_size=16,
      ).unwrap()
      inspect(written.read_all(max_bytes=16).unwrap(), content="parent")
    }
    Err(_) => ()
  }
}

///|
test "file dest writes empty files" {
  let input = file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/mulp-empty-output.txt",
    contents=Empty,
  )
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let outputs = src([input]).pipe(file_dest("/tmp")).collect(ctx).unwrap()
  inspect(outputs[0].path, content="/tmp/mulp-empty-output.txt")
  let written = file_byte_stream("/tmp/mulp-empty-output.txt", chunk_size=8).unwrap()
  inspect(written.read_all(max_bytes=8).unwrap(), content="")
}

///|
test "file dest closes writer and propagates stream errors" {
  let bytes = byte_stream(["broken"])
  ignore(bytes.read_all(max_bytes=16))
  let input = file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/mulp-stream-error.txt",
    contents=Bytes(bytes),
  )
  let before = open_file_writer_count_for_test()
  let ctx = @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
  let result = src([input]).pipe(file_dest("/tmp")).collect(ctx)
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Stream error: byte stream already consumed",
  )
  inspect(open_file_writer_count_for_test(), content="\{before}")
}
