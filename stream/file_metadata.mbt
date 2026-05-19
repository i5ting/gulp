///|
pub fn real_file_metadata(path : String) -> Result[Array[(String, String)], @core.MulpError] {
  let encoded = @utf8.decode_lossy(file_metadata_bytes(@utf8.encode(path)))
  if encoded == "" {
    Err(@core.file_system_error("cannot stat path: " + path))
  } else {
    let lines : Array[String] = []
    for line in encoded.split("\n") {
      lines.push(line.to_owned())
    }
    if lines.length() < 4 {
      Err(@core.file_system_error("cannot stat path: " + path))
    } else {
      Ok([
        ("kind", lines[0]),
        ("size", lines[1]),
        ("mtime_ms", lines[2]),
        ("mode", lines[3]),
        ("ctime_ms", if lines.length() >= 5 { lines[4] } else { lines[2] }),
      ])
    }
  }
}

///|
#borrow(path)
extern "c" fn file_metadata_bytes(path : Bytes) -> Bytes = "mulp_file_metadata_bytes"
