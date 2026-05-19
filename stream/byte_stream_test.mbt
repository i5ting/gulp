///|
test "byte stream reads chunks in order" {
  let stream = byte_stream(["a", "bc"])
  inspect(@utf8.decode_lossy(stream.read_next().unwrap().unwrap()), content="a")
  inspect(
    @utf8.decode_lossy(stream.read_next().unwrap().unwrap()),
    content="bc",
  )
  inspect(stream.read_next().unwrap() is None, content="true")
}

///|
test "byte stream can read bytes chunks without text encoding" {
  let first = Bytes::from_array([b'a', b'b'])
  let second = Bytes::from_array([b'c'])
  let stream = byte_stream_from_bytes([first, second])
  inspect(stream.read_next().unwrap().unwrap().length(), content="2")
  inspect(stream.read_all(max_bytes=1).unwrap(), content="c")
}

///|
test "read_all_bytes combines chunks without decoding" {
  let stream = byte_stream_from_bytes([
    Bytes::from_array([b'a']),
    Bytes::make(0, b'\x00'),
    Bytes::from_array([b'b', b'c']),
  ])
  inspect(stream.read_all_bytes(max_bytes=3).unwrap().length(), content="3")
}

///|
test "read_all combines chunks up to max bytes" {
  let stream = byte_stream(["ab", "cd"])
  inspect(stream.read_all(max_bytes=4).unwrap(), content="abcd")
}

///|
test "read_all fails when max bytes exceeded" {
  let stream = byte_stream(["ab", "cd"])
  let result = stream.read_all(max_bytes=3)
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Stream error: byte stream exceeded 3 bytes",
  )
}

///|
test "read_all counts utf8 bytes not characters" {
  let stream = byte_stream(["é"])
  let result = stream.read_all(max_bytes=1)
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Stream error: byte stream exceeded 1 bytes",
  )
}

///|
test "read_all allows exact utf8 byte limit" {
  let stream = byte_stream(["é"])
  inspect(stream.read_all(max_bytes=2).unwrap(), content="é")
}

///|
test "read_all combines many chunks" {
  let stream = byte_stream(["a", "b", "c", "d", "e", "f"])
  inspect(stream.read_all(max_bytes=6).unwrap(), content="abcdef")
}

///|
test "read_next errors after end of stream" {
  let stream = byte_stream(["a"])
  ignore(stream.read_next())
  inspect(stream.read_next().unwrap() is None, content="true")
  let result = stream.read_next()
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Stream error: byte stream already consumed",
  )
}

///|
test "byte stream cannot be consumed twice" {
  let stream = byte_stream(["a"])
  ignore(stream.read_all(max_bytes=10))
  let result = stream.read_next()
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Stream error: byte stream already consumed",
  )
}
