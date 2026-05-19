///|
fn join_real_dest_path(dir : String, rel : String) -> String {
  if dir.has_suffix("/") {
    dir + rel
  } else {
    dir + "/" + rel
  }
}

///|
fn resolve_real_dest_dir(out_dir : String, cwd : String?) -> String {
  if out_dir.has_prefix("/") {
    out_dir
  } else {
    match cwd {
      Some(root) => join_real_dest_path(root, out_dir)
      None => out_dir
    }
  }
}

///|
fn with_real_dest_path(file : File, path : String) -> File {
  {
    cwd: file.cwd,
    base: file.base,
    path,
    contents: file.contents,
    metadata: file.metadata,
    source_map: file.source_map(),
  }
}

///|
fn write_real_file(
  path : String,
  contents : String,
) -> Result[Unit, @core.MulpError] {
  if write_file_bytes(@utf8.encode(path), @utf8.encode(contents)) {
    Ok(())
  } else {
    Err(@core.file_system_error("cannot write file: " + path))
  }
}

///|
fn write_real_buffer(
  path : String,
  contents : Bytes,
) -> Result[Unit, @core.MulpError] {
  if write_file_bytes(@utf8.encode(path), contents) {
    Ok(())
  } else {
    Err(@core.file_system_error("cannot write file: " + path))
  }
}

///|
fn write_real_file_stream(
  path : String,
  stream : ByteStream,
) -> Result[Unit, @core.MulpError] {
  let writer = open_file_writer(@utf8.encode(path))
  if !file_writer_is_open(writer) {
    Err(@core.file_system_error("cannot write file: " + path))
  } else {
    while true {
      match stream.read_next() {
        Ok(Some(chunk)) =>
          if !file_writer_write(writer, chunk) {
            ignore(file_writer_close(writer))
            return Err(@core.file_system_error("cannot write file: " + path))
          }
        Ok(None) =>
          if file_writer_close(writer) {
            return Ok(())
          } else {
            return Err(@core.file_system_error("cannot write file: " + path))
          }
        Err(err) => {
          ignore(file_writer_close(writer))
          return Err(err)
        }
      }
    }
    Ok(())
  }
}

///|
fn write_file_contents(file : File) -> Result[Unit, @core.MulpError] {
  match file.contents {
    Text(contents) => write_real_file(file.path, contents)
    Buffer(contents) => write_real_buffer(file.path, contents)
    Bytes(stream) => write_real_file_stream(file.path, stream)
    Empty => write_real_file(file.path, "")
    Directory =>
      if make_directory_path(@utf8.encode(file.path)) {
        Ok(())
      } else {
        Err(@core.file_system_error("cannot create directory: " + file.path))
      }
    Symlink(target) =>
      if make_symlink_path(@utf8.encode(target), @utf8.encode(file.path)) {
        Ok(())
      } else {
        Err(@core.file_system_error("cannot create symlink: " + file.path))
      }
  }
}

///|
pub fn file_dest(out_dir : String) -> Transform {
  file_dest_with_options(out_dir, cwd=None)
}

///|
pub fn file_dest_with_options(out_dir : String, cwd~ : String?) -> Transform {
  fn(input : File) -> FileStream {
    let out_dir = resolve_real_dest_dir(out_dir, cwd)
    let output = with_real_dest_path(
      input,
      join_real_dest_path(out_dir, input.relative_path()),
    )
    let state : DestState = { emitted: false }
    file_stream_from_pull(fn() -> Result[File?, @core.MulpError] {
      if state.emitted {
        Ok(None)
      } else {
        match write_file_contents(output) {
          Ok(_) => {
            state.emitted = true
            Ok(Some(output))
          }
          Err(err) => Err(err)
        }
      }
    })
  }
}

///|
#borrow(path, contents)
extern "c" fn write_file_bytes(path : Bytes, contents : Bytes) -> Bool = "mulp_write_file_bytes"

///|
priv type NativeFileWriter

///|
#borrow(path)
extern "c" fn open_file_writer(path : Bytes) -> NativeFileWriter = "mulp_open_file_writer"

///|
#borrow(writer)
extern "c" fn file_writer_is_open(writer : NativeFileWriter) -> Bool = "mulp_file_writer_is_open"

///|
#borrow(writer, chunk)
extern "c" fn file_writer_write(
  writer : NativeFileWriter,
  chunk : Bytes,
) -> Bool = "mulp_file_writer_write"

///|
#borrow(writer)
extern "c" fn file_writer_close(writer : NativeFileWriter) -> Bool = "mulp_file_writer_close"

///|
pub extern "c" fn open_file_writer_count_for_test() -> Int = "mulp_file_writer_open_count_for_test"

///|
#borrow(path)
extern "c" fn make_directory_path(path : Bytes) -> Bool = "mulp_make_directory_path"

///|
#borrow(target, path)
extern "c" fn make_symlink_path(target : Bytes, path : Bytes) -> Bool = "mulp_make_symlink_path"
