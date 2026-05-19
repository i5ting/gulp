///|
fn bench_vinyl_file(index : Int) -> VinylFile {
  vinyl_file(
    cwd="/workspace",
    base="/workspace/src",
    path=Some("/workspace/src/file-\{index}.txt"),
    contents=Buffer(Bytes::make(64, b'x')),
    stat=Some(vinyl_stat(kind=File, mode=Some(0o644), mtime_ms=Some(1L))),
    metadata=[("index", "\{index}")],
  )
}

///|
test "bench vinyl construct 10000 files" (it : @bench.T) {
  it.bench(fn() {
    let files : Array[VinylFile] = []
    for index in 0..<10000 {
      files.push(bench_vinyl_file(index))
    }
    it.keep(files.length())
  })
}

///|
test "bench vinyl relative paths 10000 files" (it : @bench.T) {
  let files : Array[VinylFile] = []
  for index in 0..<10000 {
    files.push(bench_vinyl_file(index))
  }
  it.bench(fn() {
    let mut total = 0
    for file in files {
      total = total + file.relative().length()
    }
    it.keep(total)
  })
}

///|
test "bench vinyl clone 10000 buffered files" (it : @bench.T) {
  let files : Array[VinylFile] = []
  for index in 0..<10000 {
    files.push(bench_vinyl_file(index))
  }
  it.bench(fn() {
    let clones : Array[VinylFile] = []
    for file in files {
      clones.push(file.clone())
    }
    it.keep(clones.length())
  })
}

///|
test "bench vinyl clone stream replay 1000 files" (it : @bench.T) {
  let chunk = Bytes::make(1024, b'x')
  it.bench(fn() {
    let files : Array[VinylFile] = []
    for index in 0..<1000 {
      let file = vinyl_file(
        cwd="/workspace",
        base="/workspace/src",
        path=Some("/workspace/src/file-\{index}.txt"),
        contents=Stream(@stream.byte_stream_from_bytes([chunk, chunk])),
      )
      let copy = file.clone()
      files.push(copy)
    }
    it.keep(files.length())
  })
}
