///|
test "vinyl-fs codecs expose utf8 as the default encoding" {
  inspect(default_encoding(), content="utf8")
}

///|
test "vinyl-fs codecs decode utf8 and strip bom by default" {
  let bytes = Bytes::from_array([
    b'\xef', b'\xbb', b'\xbf', b'h', b'e', b'l', b'l', b'o',
  ])
  inspect(decode_text(bytes, encoding=default_encoding()), content="hello")
}

///|
test "vinyl-fs codecs preserve raw bytes when encoding is false" {
  let bytes = Bytes::from_array([b'\xef', b'\xbb', b'\xbf', b'a'])
  inspect(decode_bytes(bytes, encoding=false), content=bytes.to_string())
  inspect(decode_bytes(bytes, encoding=true), content=Bytes::from_array([b'a']).to_string())
}

///|
test "vinyl-fs codecs encode text as utf8 bytes" {
  inspect(encode_text("hello", encoding=default_encoding()), content=b"hello".to_string())
}

///|
test "decode_text utf16le decodes little-endian UTF-16 to UTF-8 string" {
  // "AB" in UTF-16 LE: 0x41 0x00 0x42 0x00
  let utf16le_bytes = Bytes::from_array([b'\x41', b'\x00', b'\x42', b'\x00'])
  let result = decode_text(utf16le_bytes, encoding="utf16le")
  inspect(result, content="AB")
}

///|
test "decode_text utf16be decodes big-endian UTF-16 to UTF-8 string" {
  // "AB" in UTF-16 BE: 0x00 0x41 0x00 0x42
  let utf16be_bytes = Bytes::from_array([b'\x00', b'\x41', b'\x00', b'\x42'])
  let result = decode_text(utf16be_bytes, encoding="utf16be")
  inspect(result, content="AB")
}

///|
test "decode_text utf16le strips BOM" {
  // BOM (0xFF 0xFE) + "A" (0x41 0x00) in UTF-16 LE
  let with_bom = Bytes::from_array([b'\xff', b'\xfe', b'\x41', b'\x00'])
  let result = decode_text(with_bom, encoding="utf16le")
  inspect(result, content="A")
}

///|
test "encode_text utf16le encodes UTF-8 string to little-endian UTF-16" {
  let result = encode_text("AB", encoding="utf16le")
  // Expected: 0x41 0x00 0x42 0x00
  inspect(result.length(), content="4")
  inspect(result[0], content="b'\\x41'")
  inspect(result[1], content="b'\\x00'")
  inspect(result[2], content="b'\\x42'")
  inspect(result[3], content="b'\\x00'")
}

///|
test "decode_text utf8 (default) still works" {
  let bytes = @utf8.encode("hello")
  let result = decode_text(bytes, encoding="utf8")
  inspect(result, content="hello")
}

///|
test "decode_src_bytes Raw leaves BOM byte intact" {
  let input = Bytes::from_array([b'\xef', b'\xbb', b'\xbf', b'h', b'i'])
  let result = decode_src_bytes(input, Raw, remove_bom=true)
  inspect(result.length(), content="5")
  inspect(result[0].to_int(), content="239")
}

///|
test "decode_src_bytes UTF8 strips BOM when remove_bom=true" {
  let input = Bytes::from_array([b'\xef', b'\xbb', b'\xbf', b'h', b'i'])
  let result = decode_src_bytes(input, UTF8, remove_bom=true)
  inspect(result.length(), content="2")
  inspect(@utf8.decode_lossy(result), content="hi")
}

///|
test "decode_src_bytes UTF8 keeps BOM when remove_bom=false" {
  let input = Bytes::from_array([b'\xef', b'\xbb', b'\xbf', b'h', b'i'])
  let result = decode_src_bytes(input, UTF8, remove_bom=false)
  inspect(result.length(), content="5")
  inspect(result[0].to_int(), content="239")
}

///|
test "decode_src_bytes Charset utf16le transcodes to utf-8" {
  // "hi" in UTF-16LE: h=0x68,0x00  i=0x69,0x00
  let input = Bytes::from_array([b'\x68', b'\x00', b'\x69', b'\x00'])
  let result = decode_src_bytes(input, Charset("utf16le"), remove_bom=false)
  inspect(@utf8.decode_lossy(result), content="hi")
}

///|
test "encode_dest_bytes UTF8 is identity" {
  let input = @utf8.encode("hello")
  let result = encode_dest_bytes(input, DestEncoding::UTF8)
  inspect(@utf8.decode_lossy(result), content="hello")
}

///|
test "encode_dest_bytes Raw is identity" {
  let input = Bytes::from_array([b'\x01', b'\x02', b'\x03'])
  let result = encode_dest_bytes(input, DestEncoding::Raw)
  inspect(result.length(), content="3")
  inspect(result[0].to_int(), content="1")
}

///|
test "encode_dest_bytes Charset utf16le transcodes from utf-8" {
  // "hi" → UTF-16LE: [0x68,0x00, 0x69,0x00]
  let input = @utf8.encode("hi")
  let result = encode_dest_bytes(input, DestEncoding::Charset("utf16le"))
  inspect(result.length(), content="4")
  inspect(result[0].to_int(), content="104")
  inspect(result[1].to_int(), content="0")
  inspect(result[2].to_int(), content="105")
  inspect(result[3].to_int(), content="0")
}
