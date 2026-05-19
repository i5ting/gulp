///|
pub struct VinylFsFileMetadata {
  kind : String
  size : String?
  mtime_ms : String?
  mode : String?
}

///|
pub fn VinylFsFileMetadata::kind(self : VinylFsFileMetadata) -> String {
  self.kind
}

///|
pub fn VinylFsFileMetadata::size(self : VinylFsFileMetadata) -> String? {
  self.size
}

///|
pub fn VinylFsFileMetadata::mtime_ms(self : VinylFsFileMetadata) -> String? {
  self.mtime_ms
}

///|
pub fn VinylFsFileMetadata::mode(self : VinylFsFileMetadata) -> String? {
  self.mode
}

///|
pub fn file_metadata(
  path : String,
) -> Result[VinylFsFileMetadata, @core.MulpError] {
  match @stream.real_file_metadata(path) {
    Ok(metadata) =>
      match metadata_value(metadata, "kind") {
        Some(kind) =>
          Ok({
            kind,
            size: metadata_value(metadata, "size"),
            mtime_ms: metadata_value(metadata, "mtime_ms"),
            mode: metadata_value(metadata, "mode"),
          })
        None => Err(@core.file_system_error("cannot stat path: " + path))
      }
    Err(err) => Err(err)
  }
}

///|
pub fn read_symlink_target_file(
  path : String,
) -> Result[String, @core.MulpError] {
  let target = read_symlink_target(path)
  if target == "" {
    Err(@core.file_system_error("cannot read symlink: " + path))
  } else {
    Ok(target)
  }
}
