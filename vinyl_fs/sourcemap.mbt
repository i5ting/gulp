///|
fn sourcemap_join_path(dir : String, name : String) -> String {
  if dir == "/" {
    "/" + name
  } else if dir.has_suffix("/") {
    dir + name
  } else {
    dir + "/" + name
  }
}

///|
fn source_map_path(file : @vinyl.VinylFile, location : String) -> String? {
  match file.path() {
    None => None
    Some(path) => {
      let map_name = file.basename() + ".map"
      if location == "." || location == "" {
        Some(path + ".map")
      } else if location.has_prefix("/") {
        Some(sourcemap_join_path(location, map_name))
      } else {
        Some(sourcemap_join_path(sourcemap_join_path(file.dirname(), location), map_name))
      }
    }
  }
}

///|
fn concat_bytes(left : Bytes, right : Bytes) -> Bytes {
  Bytes::makei(left.length() + right.length(), fn(index : Int) -> Byte {
    if index < left.length() {
      left[index]
    } else {
      right[index - left.length()]
    }
  })
}

///|
fn base64_digit(value : Int) -> String {
  let table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
  table[value:value + 1].to_owned()
}

///|
fn base64_encode(bytes : Bytes) -> String {
  let parts : Array[String] = []
  let mut index = 0
  while index < bytes.length() {
    let first = bytes[index].to_int()
    let has_second = index + 1 < bytes.length()
    let has_third = index + 2 < bytes.length()
    let second = if has_second { bytes[index + 1].to_int() } else { 0 }
    let third = if has_third { bytes[index + 2].to_int() } else { 0 }
    parts.push(base64_digit(first / 4))
    parts.push(base64_digit(((first % 4) * 16) + (second / 16)))
    parts.push(if has_second {
      base64_digit(((second % 16) * 4) + (third / 64))
    } else {
      "="
    })
    parts.push(if has_third { base64_digit(third % 64) } else { "=" })
    index = index + 3
  }
  parts.join("")
}

///|
fn inline_source_map_comment(source_map : String) -> String {
  "\n//# sourceMappingURL=data:application/json;charset=utf-8;base64," +
  base64_encode(@utf8.encode(source_map))
}

///|
fn append_stream_footer(
  stream : @stream.ByteStream,
  footer : Bytes,
) -> @stream.ByteStream {
  let mut footer_sent = false
  @stream.byte_stream_from_pull(fn() -> Result[Bytes?, @core.MulpError] {
    if footer_sent {
      Ok(None)
    } else {
      match stream.read_next() {
        Ok(Some(chunk)) => Ok(Some(chunk))
        Ok(None) => {
          footer_sent = true
          Ok(Some(footer))
        }
        Err(err) => Err(err)
      }
    }
  })
}

///|
pub fn attach_source_map(
  file : @vinyl.VinylFile,
  source_map : String,
) -> @vinyl.VinylFile {
  file.with_source_map(Some(source_map))
}

///|
pub fn inline_source_map_file(file : @vinyl.VinylFile) -> @vinyl.VinylFile {
  match file.source_map() {
    None => file
    Some(source_map) => {
      let footer = @utf8.encode(inline_source_map_comment(source_map))
      match file.contents() {
        @vinyl.Buffer(bytes) =>
          file.with_contents(@vinyl.Buffer(concat_bytes(bytes, footer)))
        @vinyl.Stream(stream) =>
          file.with_contents(@vinyl.Stream(append_stream_footer(stream, footer)))
        @vinyl.Null => file
      }
    }
  }
}

///|
pub fn source_map_file(
  file : @vinyl.VinylFile,
  location~ : String,
) -> @vinyl.VinylFile? {
  match file.source_map() {
    None => None
    Some(source_map) =>
      match source_map_path(file, location) {
        None => None
        Some(path) =>
          Some(
            @vinyl.vinyl_file(
              cwd=file.cwd(),
              base=file.base(),
              path=Some(path),
              contents=@vinyl.Buffer(@utf8.encode(source_map)),
            ),
          )
      }
  }
}
