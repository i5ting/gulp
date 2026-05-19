///|
fn bench_context() -> @core.Context {
  @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
}

///|
fn bench_vinyl_files(count : Int) -> Array[@vinyl.VinylFile] {
  let files : Array[@vinyl.VinylFile] = []
  for index in 0..<count {
    files.push(
      @vinyl.vinyl_file(
        cwd="/workspace",
        base="/workspace/src",
        path=Some("/workspace/src/file-\{index}.txt"),
        contents=@vinyl.Buffer(@utf8.encode("content-\{index}")),
      ),
    )
  }
  files
}

///|
fn bench_large_byte_stream(chunks : Int) -> @stream.ByteStream {
  @stream.byte_stream_from_bytes(
    [Bytes::make(1024 * 1024, b'x')].repeat(chunks),
  )
}

///|
test "bench vinyl fs dest 1000 buffered files" (it : @bench.T) {
  let files = bench_vinyl_files(1000)
  it.bench(fn() {
    let outputs = vinyl_file_stream(files)
      .pipe(dest("/tmp/mulp-vinyl-fs-bench-dest-buffer"))
      .collect(bench_context())
      .unwrap()
    it.keep(outputs.length())
  })
}

///|
test "bench vinyl fs dest 16MB stream" (it : @bench.T) {
  it.bench(fn() {
    let input = @vinyl.vinyl_file(
      cwd="/workspace",
      base="/workspace/src",
      path=Some("/workspace/src/bench-large.txt"),
      contents=@vinyl.Stream(bench_large_byte_stream(16)),
    )
    let outputs = vinyl_file_stream([input])
      .pipe(dest("/tmp/mulp-vinyl-fs-bench-dest-stream"))
      .collect(bench_context())
      .unwrap()
    it.keep(outputs.length())
  })
}

///|
test "bench vinyl fs stream pipeline 1000 files" (it : @bench.T) {
  let files = bench_vinyl_files(1000)
  it.bench(fn() {
    let outputs = vinyl_file_stream(files)
      .pipe(dest("/tmp/mulp-vinyl-fs-bench-pipeline"))
      .collect(bench_context())
      .unwrap()
    it.keep(outputs.length())
  })
}
