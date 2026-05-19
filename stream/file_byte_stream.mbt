///|
priv type NativeFileReader

///|
pub fn file_byte_stream(
  path : String,
  chunk_size~ : Int,
) -> Result[ByteStream, @core.MulpError] {
  file_byte_stream_with_cancel_check(path, chunk_size~, fn() -> Bool { false })
}

///|
pub fn file_byte_stream_with_context(
  path : String,
  chunk_size~ : Int,
  ctx : @core.Context,
) -> Result[ByteStream, @core.MulpError] {
  file_byte_stream_with_cancel_check(path, chunk_size~, fn() -> Bool {
    ctx.is_cancelled()
  })
}

///|
fn file_byte_stream_with_cancel_check(
  path : String,
  chunk_size~ : Int,
  is_cancelled : () -> Bool,
) -> Result[ByteStream, @core.MulpError] {
  if chunk_size <= 0 {
    Err(@core.stream_error("file byte stream chunk size must be positive"))
  } else {
    let reader = open_file_reader(@utf8.encode(path), chunk_size)
    if !file_reader_is_open(reader) {
      Err(@core.file_system_error("cannot open file: " + path))
    } else {
      Ok(
        byte_stream_from_pull_with_close(
          fn() -> Result[Bytes?, @core.MulpError] {
            if is_cancelled() {
              Err(@core.task_failed("stream", "cancelled"))
            } else {
              let chunk = file_reader_read(reader)
              if chunk.length() == 0 {
                Ok(None)
              } else {
                Ok(Some(chunk))
              }
            }
          },
          fn() -> Unit { file_reader_close(reader) },
        ),
      )
    }
  }
}

///|
#borrow(path)
extern "c" fn open_file_reader(
  path : Bytes,
  chunk_size : Int,
) -> NativeFileReader = "mulp_open_file_reader"

///|
#borrow(reader)
extern "c" fn file_reader_is_open(reader : NativeFileReader) -> Bool = "mulp_file_reader_is_open"

///|
#borrow(reader)
extern "c" fn file_reader_read(reader : NativeFileReader) -> Bytes = "mulp_file_reader_read"

///|
#borrow(reader)
extern "c" fn file_reader_close(reader : NativeFileReader) -> Unit = "mulp_file_reader_close"

///|
pub extern "c" fn open_file_reader_count_for_test() -> Int = "mulp_file_reader_open_count_for_test"
