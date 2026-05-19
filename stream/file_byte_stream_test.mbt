///|
test "file byte stream reads a real file in chunks" {
  let stream = file_byte_stream("stream/fixtures/chunked.txt", chunk_size=2).unwrap()
  inspect(
    @utf8.decode_lossy(stream.read_next().unwrap().unwrap()),
    content="ab",
  )
  inspect(
    @utf8.decode_lossy(stream.read_next().unwrap().unwrap()),
    content="cd",
  )
  inspect(
    @utf8.decode_lossy(stream.read_next().unwrap().unwrap()),
    content="ef",
  )
  inspect(
    @utf8.decode_lossy(stream.read_next().unwrap().unwrap()),
    content="\n",
  )
  inspect(stream.read_next().unwrap() is None, content="true")
}

///|
test "file byte stream rejects missing files" {
  let result = file_byte_stream("stream/fixtures/missing.txt", chunk_size=2)
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="File system error: cannot open file: stream/fixtures/missing.txt",
  )
}

///|
test "file byte stream closes open reader when context is cancelled" {
  let token = @core.new_cancellation_token()
  let ctx = @core.new_context(cwd="/workspace", now_ms=0L, cancellation=token)
  let before = open_file_reader_count_for_test()
  let stream = file_byte_stream_with_context(
    "stream/fixtures/chunked.txt",
    chunk_size=2,
    ctx,
  ).unwrap()
  inspect(open_file_reader_count_for_test() == before + 1, content="true")
  token.cancel()
  let result = stream.read_next()
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Task failed: stream: cancelled",
  )
  inspect(open_file_reader_count_for_test(), content="\{before}")
}

///|
test "file byte stream closes reader when downstream stops early" {
  let before = open_file_reader_count_for_test()
  let stream = file_byte_stream("stream/fixtures/chunked.txt", chunk_size=2).unwrap()
  inspect(open_file_reader_count_for_test() == before + 1, content="true")
  ignore(stream.read_next())
  stream.close()
  inspect(open_file_reader_count_for_test(), content="\{before}")
}

///|
test "file byte stream reads large files incrementally" {
  let stream = file_byte_stream("stream/fixtures/large.txt", chunk_size=128).unwrap()
  let mut chunks = 0
  let mut total = 0
  while true {
    match stream.read_next().unwrap() {
      Some(chunk) => {
        chunks = chunks + 1
        total = total + chunk.length()
        inspect(chunk.length() <= 128, content="true")
      }
      None => break
    }
  }
  inspect(chunks, content="9")
  inspect(total, content="1040")
}
