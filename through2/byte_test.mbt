///|
struct Counter {
  mut value : Int
}

///|
test "through2-master classicNoopTransform maps to byte_passthrough" {
  let input = @stream.byte_stream(["ee", "ee"])
  let output = byte_passthrough()(input)
  inspect(output.read_all(max_bytes=8).unwrap(), content="eeee")
}

///|
test "through2-master asyncReturnedValuePushed maps byte chunks" {
  let input = @stream.byte_stream(["a", "b"])
  let output = through_bytes(fn(
    _chunk : Bytes,
  ) -> Result[Bytes?, @core.MulpError] {
    Ok(Some(@utf8.encode("X")))
  })(input)
  inspect(output.read_all(max_bytes=8).unwrap(), content="XX")
}

///|
test "through2-master classicByteTransform maps to stateful byte map" {
  let counter : Counter = { value: 96 }
  let input = @stream.byte_stream(["xxxxxxxxxx", "xxxxx", "xxxxxxxxxx"])
  let output = through_bytes(fn(
    chunk : Bytes,
  ) -> Result[Bytes?, @core.MulpError] {
    counter.value = counter.value + 1
    Ok(Some(Bytes::make(chunk.length(), counter.value.to_byte())))
  })(input)
  inspect(
    output.read_all(max_bytes=32).unwrap(),
    content="aaaaaaaaaabbbbbcccccccccc",
  )
}

///|
test "through2-master asyncUndefinedSkipsEmission maps None to filter" {
  let mut index = 0
  let input = @stream.byte_stream(["0", "1", "2", "3", "4"])
  let output = through_bytes(fn(
    chunk : Bytes,
  ) -> Result[Bytes?, @core.MulpError] {
    let keep = index % 2 == 0
    index = index + 1
    if keep {
      Ok(Some(chunk))
    } else {
      Ok(None)
    }
  })(input)
  inspect(output.read_all(max_bytes=8).unwrap(), content="024")
}

///|
test "through2-master asyncgenOneToOne maps to byte flatMap one-to-one" {
  let input = @stream.byte_stream(["a", "b"])
  let output = through_bytes_flat(fn(
    chunk : Bytes,
  ) -> Result[Array[Bytes], @core.MulpError] {
    Ok([chunk])
  })(input)
  inspect(output.read_all(max_bytes=8).unwrap(), content="ab")
}

///|
test "through2-master asyncgenFanOut maps to byte flatMap" {
  let input = @stream.byte_stream(["a", "b"])
  let output = through_bytes_flat(fn(
    chunk : Bytes,
  ) -> Result[Array[Bytes], @core.MulpError] {
    Ok([chunk, chunk])
  })(input)
  inspect(output.read_all(max_bytes=8).unwrap(), content="aabb")
}

///|
test "through2-master classicFlush maps to byte flush" {
  let input = @stream.byte_stream(["a", "b"])
  let output = through_bytes_with_flush(
    fn(chunk : Bytes) -> Result[Bytes?, @core.MulpError] { Ok(Some(chunk)) },
    fn() -> Result[Array[Bytes], @core.MulpError] { Ok([@utf8.encode("end")]) },
  )(input)
  inspect(output.read_all(max_bytes=16).unwrap(), content="abend")
}

///|
test "through2-master asyncgenWithClassicFlush maps flatMap plus flush" {
  let input = @stream.byte_stream(["a", "b"])
  let output = through_bytes_flat_with_flush(
    fn(chunk : Bytes) -> Result[Array[Bytes], @core.MulpError] { Ok([chunk]) },
    fn() -> Result[Array[Bytes], @core.MulpError] { Ok([@utf8.encode("END")]) },
  )(input)
  inspect(output.read_all(max_bytes=16).unwrap(), content="abEND")
}

///|
test "through2-master asyncgenThrownPropagates maps flatMap errors" {
  let input = @stream.byte_stream(["x"])
  let output = through_bytes_flat(fn(
    _chunk : Bytes,
  ) -> Result[Array[Bytes], @core.MulpError] {
    Err(@core.stream_error("gen-boom"))
  })(input)
  let result = output.read_all(max_bytes=8)
  inspect(result is Err(_), content="true")
  inspect(result.unwrap_err().message(), content="Stream error: gen-boom")
}

///|
test "through2-master asyncRejectedPropagatesError maps transform errors" {
  let input = @stream.byte_stream(["x"])
  let output = through_bytes(fn(
    _chunk : Bytes,
  ) -> Result[Bytes?, @core.MulpError] {
    Err(@core.stream_error("boom"))
  })(input)
  let result = output.read_all(max_bytes=8)
  inspect(result is Err(_), content="true")
  inspect(result.unwrap_err().message(), content="Stream error: boom")
}

///|
test "through2 flush errors propagate" {
  let input = @stream.byte_stream([])
  let output = through_bytes_with_flush(
    fn(chunk : Bytes) -> Result[Bytes?, @core.MulpError] { Ok(Some(chunk)) },
    fn() -> Result[Array[Bytes], @core.MulpError] {
      Err(@core.stream_error("flush-boom"))
    },
  )(input)
  let result = output.read_all(max_bytes=8)
  inspect(result is Err(_), content="true")
  inspect(result.unwrap_err().message(), content="Stream error: flush-boom")
}

///|
test "through2 pull based transforms are lazy" {
  let pulls : Counter = { value: 0 }
  let input = @stream.byte_stream_from_pull(fn() -> Result[
    Bytes?,
    @core.MulpError,
  ] {
    pulls.value = pulls.value + 1
    if pulls.value == 1 {
      Ok(Some(@utf8.encode("a")))
    } else {
      Ok(None)
    }
  })
  let output = byte_passthrough()(input)
  inspect(pulls.value, content="0")
  inspect(output.read_next().unwrap().unwrap(), content=b"a".to_string())
  inspect(pulls.value, content="1")
}

///|
test "through2 closing byte output closes upstream" {
  let closed : Counter = { value: 0 }
  let input = @stream.byte_stream_from_pull_with_close(
    fn() -> Result[Bytes?, @core.MulpError] { Ok(Some(@utf8.encode("a"))) },
    fn() -> Unit { closed.value = closed.value + 1 },
  )
  let output = byte_passthrough()(input)
  output.close()
  inspect(closed.value, content="1")
}

///|
test "through2 byte_transformer creates independent state" {
  let make = byte_transformer(
    fn(chunk : Bytes) -> Result[Bytes?, @core.MulpError] { Ok(Some(chunk)) },
    flush=Some(fn() -> Result[Array[Bytes], @core.MulpError] {
      Ok([@utf8.encode("!")])
    }),
  )
  let first = make()(@stream.byte_stream(["a"]))
  let second = make()(@stream.byte_stream(["b"]))
  inspect(first.read_all(max_bytes=8).unwrap(), content="a!")
  inspect(second.read_all(max_bytes=8).unwrap(), content="b!")
}
