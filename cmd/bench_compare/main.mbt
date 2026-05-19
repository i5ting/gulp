///|
fn context() -> @core.Context {
  @core.new_context(
    cwd="/workspace",
    now_ms=0L,
    cancellation=@core.new_cancellation_token(),
  )
}

///|
fn files(count : Int) -> Array[@stream.File] {
  let files : Array[@stream.File] = []
  for index in 0..<count {
    files.push(
      @stream.file(
        cwd="/workspace",
        base="/workspace/src",
        path="/workspace/src/file-\{index}.txt",
        contents=@stream.Buffer(@utf8.encode("content-\{index}")),
      ),
    )
  }
  files
}

///|
fn large_byte_stream(chunks : Int) -> @stream.ByteStream {
  @stream.byte_stream_from_bytes(
    [Bytes::make(1024 * 1024, b'x')].repeat(chunks),
  )
}

///|
fn vinyl_files(count : Int) -> Array[@vinyl.VinylFile] {
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
fn pipeline_object_sink_once() -> Int {
  @stream.src(files(1000))
  .pipe(
    @stream.map_files(fn(source : @stream.File) -> @stream.File {
      source.with_path(source.path + ".copy")
    }),
  )
  .collect(context())
  .unwrap()
  .length()
}

///|
fn pipeline_memory_dest_once() -> Int {
  let sink = @stream.memory_dest()
  @stream.src(files(1000))
  .pipe(
    @stream.map_files(fn(source : @stream.File) -> @stream.File {
      source.with_path(source.path + ".copy")
    }),
  )
  .pipe(@stream.dest("dist", sink))
  .collect(context())
  .unwrap()
  .length()
}

///|
fn dest_stream_16mb_once() -> Int {
  let input = @stream.file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/bench-large.txt",
    contents=@stream.Bytes(large_byte_stream(16)),
  )
  @stream.src([input])
  .pipe(@stream.file_dest("/tmp/mulp-stable-bench"))
  .collect(context())
  .unwrap()
  .length()
}

///|
fn dest_buffer_16mb_once() -> Int {
  let input = @stream.file(
    cwd="/workspace",
    base="/workspace/src",
    path="/workspace/src/bench-large.txt",
    contents=@stream.Buffer(Bytes::make(16 * 1024 * 1024, b'x')),
  )
  @stream.src([input])
  .pipe(@stream.file_dest("/tmp/mulp-stable-bench"))
  .collect(context())
  .unwrap()
  .length()
}

///|
fn vinyl_construct_10000_once() -> Int {
  vinyl_files(10000).length()
}

///|
fn vinyl_relative_10000_once() -> Int {
  let files = vinyl_files(10000)
  let mut total = 0
  for file in files {
    total = total + file.relative().length()
  }
  total
}

///|
fn vinyl_clone_buffer_10000_once() -> Int {
  let files = vinyl_files(10000)
  let clones : Array[@vinyl.VinylFile] = []
  for file in files {
    clones.push(file.clone())
  }
  clones.length()
}

///|
fn vinyl_fs_src_buffer_1000_once() -> Int {
  @vinyl_fs.src(
    ["**/*.txt"],
    @vinyl_fs.src_options(cwd="/tmp/mulp-vinyl-fs-bench-src"),
  )
  .collect(context())
  .unwrap()
  .length()
}

///|
fn vinyl_fs_dest_buffer_1000_once() -> Int {
  @vinyl_fs.vinyl_file_stream(vinyl_files(1000))
  .pipe(@vinyl_fs.dest("/tmp/mulp-stable-dest-buffer"))
  .collect(context())
  .unwrap()
  .length()
}

///|
fn vinyl_fs_dest_stream_16mb_once() -> Int {
  let input = @vinyl.vinyl_file(
    cwd="/workspace",
    base="/workspace/src",
    path=Some("/workspace/src/bench-large.txt"),
    contents=@vinyl.Stream(large_byte_stream(16)),
  )
  @vinyl_fs.vinyl_file_stream([input])
  .pipe(@vinyl_fs.dest("/tmp/mulp-stable-dest-stream"))
  .collect(context())
  .unwrap()
  .length()
}

///|
fn run_sample(scenario : String) -> Int {
  match scenario {
    "pipeline-object-sink" => pipeline_object_sink_once()
    "pipeline-memory-dest" => pipeline_memory_dest_once()
    "dest-stream-16mb" => dest_stream_16mb_once()
    "dest-buffer-16mb" => dest_buffer_16mb_once()
    "vinyl-construct-10000" => vinyl_construct_10000_once()
    "vinyl-relative-10000" => vinyl_relative_10000_once()
    "vinyl-clone-buffer-10000" => vinyl_clone_buffer_10000_once()
    "vinyl-fs-src-buffer-1000" => vinyl_fs_src_buffer_1000_once()
    "vinyl-fs-dest-buffer-1000" => vinyl_fs_dest_buffer_1000_once()
    "vinyl-fs-dest-stream-16mb" => vinyl_fs_dest_stream_16mb_once()
    _ => 0
  }
}

///|
fn main {
  let args = @env.args()
  let scenario = if args.length() >= 2 {
    args[1]
  } else {
    "pipeline-memory-dest"
  }
  let warmup = 10
  let samples = 40
  let mut checksum = 0
  for _ in 0..<warmup {
    checksum = checksum + run_sample(scenario)
  }
  println("scenario=" + scenario)
  println("warmup=\{warmup}")
  println("samples=\{samples}")
  for _ in 0..<samples {
    let started = @bench.monotonic_clock_start()
    checksum = checksum + run_sample(scenario)
    let elapsed_ms = @bench.monotonic_clock_end(started) / 1000.0
    println("\{elapsed_ms}")
  }
  println("checksum=\{checksum}")
}
