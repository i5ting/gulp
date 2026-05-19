///|
fn bench_context() -> @core.Context {
  @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
}

///|
fn bench_files(count : Int) -> Array[File] {
  let files : Array[File] = []
  for index in 0..<count {
    files.push(
      file(
        cwd="/workspace",
        base="/workspace/src",
        path="/workspace/src/file-\{index}.txt",
        contents=Buffer(@utf8.encode("content-\{index}")),
      ),
    )
  }
  files
}

///|
fn bench_large_byte_stream(chunks : Int) -> ByteStream {
  byte_stream_from_bytes([Bytes::make(1024 * 1024, b'x')].repeat(chunks))
}

///|
test "bench stream pipeline 1000 buffered files" (it : @bench.T) {
  let files = bench_files(1000)
  it.bench(fn() {
    let sink = memory_dest()
    let outputs = src(files)
      .pipe(
        map_files(fn(source : File) -> File {
          source.with_path(source.path + ".copy")
        }),
      )
      .pipe(dest("dist", sink))
      .collect(bench_context())
      .unwrap()
    it.keep(outputs.length())
  })
}

///|
test "bench file dest 16MB byte stream" (it : @bench.T) {
  it.bench(fn() {
    let input = file(
      cwd="/workspace",
      base="/workspace/src",
      path="/workspace/src/bench-large.txt",
      contents=Bytes(bench_large_byte_stream(16)),
    )
    let outputs = src([input])
      .pipe(file_dest("/tmp/mulp-bench-dest"))
      .collect(bench_context())
      .unwrap()
    it.keep(outputs.length())
  })
}
