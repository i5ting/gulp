///|
pub fn default_encoding() -> String {
  "utf8"
}

///|
fn has_utf8_bom(bytes : Bytes) -> Bool {
  bytes.length() >= 3 &&
  bytes[0] == b'\xef' &&
  bytes[1] == b'\xbb' &&
  bytes[2] == b'\xbf'
}

///|
pub fn strip_bom(bytes : Bytes) -> Bytes {
  if has_utf8_bom(bytes) {
    Bytes::makei(bytes.length() - 3, fn(index : Int) -> Byte {
      bytes[index + 3]
    })
  } else {
    bytes
  }
}

///|
pub fn decode_bytes(bytes : Bytes, encoding~ : Bool) -> Bytes {
  if encoding {
    strip_bom(bytes)
  } else {
    bytes
  }
}

///|
pub fn decode_text(bytes : Bytes, encoding~ : String) -> String {
  match encoding {
    "utf16le" | "utf-16le" | "ucs2" | "ucs-2" => utf16le_to_utf8(bytes)
    "utf16be" | "utf-16be" => utf16be_to_utf8(bytes)
    _ => @utf8.decode_lossy(strip_bom(bytes))
  }
}

///|
pub fn encode_text(text : String, encoding~ : String) -> Bytes {
  match encoding {
    "utf16le" | "utf-16le" | "ucs2" | "ucs-2" => utf8_to_utf16le(text)
    "utf16be" | "utf-16be" => utf8_to_utf16be(text)
    _ => @utf8.encode(text)
  }
}

///|
fn utf16le_to_utf8(bytes : Bytes) -> String {
  let start = if bytes.length() >= 2 &&
    bytes[0] == b'\xff' &&
    bytes[1] == b'\xfe' {
    2
  } else {
    0
  }
  let chars : Array[Char] = []
  let mut i = start
  while i + 1 < bytes.length() {
    let low = bytes[i].to_int()
    let high = bytes[i + 1].to_int()
    let codepoint = high * 256 + low
    chars.push(codepoint.unsafe_to_char())
    i = i + 2
  }
  String::from_array(chars)
}

///|
fn utf16be_to_utf8(bytes : Bytes) -> String {
  let start = if bytes.length() >= 2 &&
    bytes[0] == b'\xfe' &&
    bytes[1] == b'\xff' {
    2
  } else {
    0
  }
  let chars : Array[Char] = []
  let mut i = start
  while i + 1 < bytes.length() {
    let high = bytes[i].to_int()
    let low = bytes[i + 1].to_int()
    let codepoint = high * 256 + low
    chars.push(codepoint.unsafe_to_char())
    i = i + 2
  }
  String::from_array(chars)
}

///|
fn utf8_to_utf16le(text : String) -> Bytes {
  let buf : Array[Byte] = []
  for ch in text {
    let cp = ch.to_int()
    buf.push((cp % 256).to_byte())
    buf.push((cp / 256).to_byte())
  }
  Bytes::from_array(buf)
}

///|
fn utf8_to_utf16be(text : String) -> Bytes {
  let buf : Array[Byte] = []
  for ch in text {
    let cp = ch.to_int()
    buf.push((cp / 256).to_byte())
    buf.push((cp % 256).to_byte())
  }
  Bytes::from_array(buf)
}

///|
pub(all) enum SrcEncoding {
  UTF8
  Raw
  Charset(String)
}

///|
pub(all) enum DestEncoding {
  UTF8
  Raw
  Charset(String)
}

///|
/// Decode source bytes using the given encoding.
/// UTF8: strip BOM if remove_bom is true, otherwise return unchanged.
/// Raw: always return bytes unchanged.
/// Charset(cs): transcode via the charset helpers then re-encode as UTF-8.
pub fn decode_src_bytes(
  bytes      : Bytes,
  encoding   : SrcEncoding,
  remove_bom~ : Bool,
) -> Bytes {
  match encoding {
    Raw            => bytes
    UTF8           => if remove_bom { strip_bom(bytes) } else { bytes }
    Charset(cs)    => @utf8.encode(decode_text(bytes, encoding=cs))
  }
}

///|
/// Encode bytes for writing using the given destination encoding.
/// UTF8 and Raw: return bytes unchanged.
/// Charset(cs): decode bytes as UTF-8 text and re-encode to the charset.
pub fn encode_dest_bytes(bytes : Bytes, encoding : DestEncoding) -> Bytes {
  match encoding {
    UTF8 | Raw   => bytes
    Charset(cs)  => encode_text(@utf8.decode_lossy(bytes), encoding=cs)
  }
}
