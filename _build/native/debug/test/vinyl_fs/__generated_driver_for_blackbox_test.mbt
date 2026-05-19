// #region Define traits with default implementations

///|
/// is_being_cancelled may be unused if there's no async test
#warnings("-unused_value")
trait MoonBit_Async_Test_Driver {
  run_async_tests(@moonbitlang/core/prelude.Array[async () -> Unit]) -> Unit = _
  is_being_cancelled() -> Bool = _
}

///|
impl MoonBit_Async_Test_Driver with run_async_tests(_) {
  ()
}

///|
impl MoonBit_Async_Test_Driver with is_being_cancelled() {
  false
}

///|
trait MoonBit_Test_Driver {
  /// returns if the test is found and executed
  run_test(
    @moonbitlang/core/prelude.Array[async () -> Unit],
    String,
    Int,
    (String, String, Bool) -> Unit,
    (Error) -> String,
  ) -> Bool raise MoonBitTestDriverInternalSkipTest = _
}

///|
impl MoonBit_Test_Driver with run_test(_, _, _, _, _) {
  false
}

// #endregion
// #region Define abstract types for namespace with implementation declarations

///|
priv type MoonBit_Test_Driver_Internal_No_Args

///|
impl MoonBit_Test_Driver for MoonBit_Test_Driver_Internal_No_Args

///|
priv type MoonBit_Test_Driver_Internal_With_Args

///|
impl MoonBit_Test_Driver for MoonBit_Test_Driver_Internal_With_Args

///|
priv type MoonBit_Test_Driver_Internal_Async_No_Args

///|
impl MoonBit_Test_Driver for MoonBit_Test_Driver_Internal_Async_No_Args

///|
priv type MoonBit_Test_Driver_Internal_Async_With_Args

///|
impl MoonBit_Test_Driver for MoonBit_Test_Driver_Internal_Async_With_Args

///|
priv type MoonBit_Test_Driver_Internal_With_Bench_Args

///|
impl MoonBit_Test_Driver for MoonBit_Test_Driver_Internal_With_Bench_Args

///|
priv type MoonBit_Async_Test_Driver_Impl

///|
impl MoonBit_Async_Test_Driver for MoonBit_Async_Test_Driver_Impl

// #endregion

// #region Utility Types

///|
/// Maybe unused if there's no tests
#warnings("-unused_type_declaration")
priv struct MoonBitTestDriverInternalTestMap[F](
  @moonbitlang/core/prelude.Map[
    String,
    @moonbitlang/core/prelude.Map[Int, (F, ReadOnlyArray[String])],
  ]
)

///|
priv suberror MoonBitTestDriverInternalSkipTest {
  MoonBitTestDriverInternalSkipTest(name~ : String)
}

// #endregion
///|
fn moonbit_test_driver_internal_error_to_string(x : Error) -> String = "%error.to_string"

///|
#cfg(target="native")
fn string_get(s : String, index : Int) -> Int = "%string_get"

///|
#cfg(not(target="js"))
fn moonbit_unsafe_char_from_int(x : Int) -> Char = "%identity"

///|
#cfg(target="native")
const MOONBIT_TEST_DRIVER_INTERNAL_IS_NATIVE = true

///|
#cfg(not(target="native"))
const MOONBIT_TEST_DRIVER_INTERNAL_IS_NATIVE = false

// =========================================================
// ================== BEGIN WASM SPECIFIC ==================
// =========================================================

///|
#cfg(any(target="wasm", target="wasm-gc"))
fn moonbit_test_driver_internal_get_file_name(
  file_name : MoonbitTestDriverInternalExternString,
) -> String {
  moonbit_test_driver_internal_string_from_extern(file_name)
}

///|
#cfg(any(target="wasm", target="wasm-gc"))
#external
type MoonbitTestDriverInternalStringReadHandle

///|
#cfg(any(target="wasm", target="wasm-gc"))
#external
type MoonbitTestDriverInternalExternString

///|
#cfg(any(target="wasm", target="wasm-gc"))
fn moonbit_test_driver_internal_begin_read_string(
  s : MoonbitTestDriverInternalExternString,
) -> MoonbitTestDriverInternalStringReadHandle = "__moonbit_fs_unstable" "begin_read_string"

///|
#cfg(any(target="wasm", target="wasm-gc"))
fn moonbit_test_driver_internal_string_read_char(
  handle : MoonbitTestDriverInternalStringReadHandle,
) -> Int = "__moonbit_fs_unstable" "string_read_char"

///|
#cfg(any(target="wasm", target="wasm-gc"))
fn moonbit_test_driver_internal_finish_read_string(
  handle : MoonbitTestDriverInternalStringReadHandle,
) = "__moonbit_fs_unstable" "finish_read_string"

///|
#cfg(any(target="wasm", target="wasm-gc"))
fn fixedarray_to_bytes(arr : FixedArray[Byte]) -> Bytes = "%identity"

///|
#cfg(any(target="wasm", target="wasm-gc"))
fn moonbit_test_driver_internal_string_from_extern(
  e : MoonbitTestDriverInternalExternString,
) -> String {
  let buf = FixedArray::make(512, Byte::default())
  let mut len = 0
  let handle = moonbit_test_driver_internal_begin_read_string(e)
  while true {
    let ch = moonbit_test_driver_internal_string_read_char(handle)
    if ch == -1 {
      break
    } else {
      let written_bytes_len = buf.set_utf16le_char(
        len,
        moonbit_unsafe_char_from_int(ch),
      )
      len += written_bytes_len
    }
  }
  moonbit_test_driver_internal_finish_read_string(handle)
  fixedarray_to_bytes(buf).to_unchecked_string(offset=0, length=len).to_string()
}

// =========================================================
// =================== END WASM SPECIFIC ===================
// =========================================================

// =========================================================
// ================= BEGIN NATIVE SPECIFIC =================
// =========================================================

///|
#cfg(target="native")
fn moonbit_test_driver_internal_native_parse_args() -> @moonbitlang/core/prelude.Array[
  (String, Int),
] {
  fn moonbit_test_driver_internal_parse_int_(s : String) -> Int {
    let mut res = 0
    let len = s.length()
    for i = 0; i < len; i = i + 1 {
      res = res * 10 + (string_get(s, i) - '0')
    }
    res
  }

  fn moonbit_test_driver_internal_utf8_bytes_to_mbt_string(
    bytes : Bytes,
  ) -> String {
    let res = @moonbitlang/core/prelude.StringBuilder::new()
    let len = bytes.length()
    let mut i = 0
    while i < len {
      let mut c = bytes[i].to_int()
      if c < 0x80 {
        res.write_char(moonbit_unsafe_char_from_int(c))
        i += 1
      } else if c < 0xE0 {
        if i + 1 >= len {
          break
        }
        c = ((c & 0x1F) << 6) | (bytes[i + 1].to_int() & 0x3F)
        res.write_char(moonbit_unsafe_char_from_int(c))
        i += 2
      } else if c < 0xF0 {
        if i + 2 >= len {
          break
        }
        c = ((c & 0x0F) << 12) |
          ((bytes[i + 1].to_int() & 0x3F) << 6) |
          (bytes[i + 2].to_int() & 0x3F)
        res.write_char(moonbit_unsafe_char_from_int(c))
        i += 3
      } else {
        if i + 3 >= len {
          break
        }
        c = ((c & 0x07) << 18) |
          ((bytes[i + 1].to_int() & 0x3F) << 12) |
          ((bytes[i + 2].to_int() & 0x3F) << 6) |
          (bytes[i + 3].to_int() & 0x3F)
        c -= 0x10000
        res.write_char(moonbit_unsafe_char_from_int((c >> 10) + 0xD800))
        res.write_char(moonbit_unsafe_char_from_int((c & 0x3FF) + 0xDC00))
        i += 4
      }
    }
    res.to_string()
  }

  fn moonbit_test_driver_internal_get_cli_args_internal() -> @moonbitlang/core/prelude.Array[
    String,
  ] {
    let tmp = moonbit_test_driver_internal_get_cli_args_ffi()
    let res = @moonbitlang/core/prelude.Array::new(capacity=tmp.length())
    for i = 0; i < tmp.length(); i = i + 1 {
      res.push(moonbit_test_driver_internal_utf8_bytes_to_mbt_string(tmp[i]))
    }
    res
  }

  fn moonbit_test_driver_internal_split_mbt_string(
    s : String,
    sep : Char,
  ) -> @moonbitlang/core/prelude.Array[String] {
    let res = []
    let mut i = 0
    let mut start = 0
    while i < s.length() {
      if moonbit_unsafe_char_from_int(string_get(s, i)) == sep {
        res.push(s.unsafe_substring(start~, end=i))
        start = i + 1
      }
      i += 1
    }
    if start < s.length() {
      res.push(s.unsafe_substring(start~, end=s.length()))
    }
    res
  }

  let file_and_index = []
  let cli_args = moonbit_test_driver_internal_get_cli_args_internal()
  let test_args = moonbit_test_driver_internal_split_mbt_string(
    cli_args[1],
    '/',
  )
  for arg in test_args {
    let file_and_range = moonbit_test_driver_internal_split_mbt_string(arg, ':')
    let file = file_and_range[0]
    let range = file_and_range[1]
    let start_and_end = moonbit_test_driver_internal_split_mbt_string(
      range, '-',
    )
    let start = moonbit_test_driver_internal_parse_int_(start_and_end[0])
    let end = moonbit_test_driver_internal_parse_int_(start_and_end[1])
    for i = start; i < end; i = i + 1 {
      file_and_index.push((file, i))
    }
  }
  file_and_index
}

///|
#cfg(target="native")
fn moonbit_test_driver_internal_get_cli_args_ffi() -> FixedArray[Bytes] = "$moonbit.get_cli_args"

// =========================================================
// ================== END NATIVE SPECIFIC ==================
// =========================================================

// =========================================================
// -================== BEGIN JS SPECIFIC ===================
// =========================================================

///|
#cfg(target="js")
#external
type MoonBitTestDriverInternalTestEntry

///|
#cfg(target="js")
extern "js" fn MoonBitTestDriverInternalTestEntry::filename(
  self : Self,
) -> String =
  #| (entry) => entry[0]

///|
#cfg(target="js")
extern "js" fn MoonBitTestDriverInternalTestEntry::index(self : Self) -> Int =
  #| (entry) => entry[1]

///|
pub(all) suberror MoonBitTestDriverInternalJsError {
  MoonBitTestDriverInternalJsError(String)
}

///|
/// Maybe unused if there's no tests
#cfg(target="js")
#warnings("-unused_value")
fn moonbit_test_driver_internal_catch_error(
  f : () -> Unit raise,
  on_ok~ : () -> Unit,
  on_err~ : (Error) -> Unit,
) -> Unit {
  moonbit_test_driver_internal_js_catch(
    on_err=err_str => on_err(MoonBitTestDriverInternalJsError(err_str)),
    () => {
      try f() catch {
        err => on_err(err)
      } noraise {
        _ => on_ok()
      }
    },
  )
}

///|
#cfg(target="js")
extern "js" fn moonbit_test_driver_internal_js_catch(
  f : () -> Unit,
  on_err~ : (String) -> Unit,
) =
  #| (f, on_err) => {
  #|   try {
  #|     f()
  #|   } catch (err) {
  #|     const msg = err.stack.toString()
  #|     on_err(msg)
  #|   }
  #| }

///|
/// Maybe unused if there's no tests
#cfg(not(target="js"))
#warnings("-unused_value")
fn moonbit_test_driver_internal_catch_error(
  f : () -> Unit raise,
  on_ok~ : () -> Unit,
  on_err~ : (Error) -> Unit,
) -> Unit {
  try f() catch {
    err => on_err(err)
  } noraise {
    _ => on_ok()
  }
}

// =========================================================
// -=================== END JS SPECIFIC ====================
// =========================================================
// This file is introduced when tests without args is needed

///|
let moonbit_test_driver_internal_no_args_tests : MoonBitTestDriverInternalTestMap[
  () -> Unit raise,
] = {
  "vinyl_fs_bench_test.mbt": {
  },
  "symlink_test.mbt": {
    0: (__test_73796d6c696e6b5f746573742e6d6274_0, ["symlink writes links to input file paths"]),
    1: (__test_73796d6c696e6b5f746573742e6d6274_1, ["symlink emits symbolic vinyl objects with preserved target"]),
    2: (__test_73796d6c696e6b5f746573742e6d6274_2, ["symlink can write relative symlink targets"]),
  },
  "src_test.mbt": {
    0: (__test_7372635f746573742e6d6274_0, ["src reads matched files from cwd using vinyl relative paths"]),
    1: (__test_7372635f746573742e6d6274_1, ["src reports invalid empty globs and missing literal files"]),
    2: (__test_7372635f746573742e6d6274_2, ["src allowEmpty permits missing literal files"]),
    3: (__test_7372635f746573742e6d6274_3, ["src supports negated globs and dot option"]),
    4: (__test_7372635f746573742e6d6274_4, ["src applies negated globs in order"]),
    5: (__test_7372635f746573742e6d6274_5, ["src streams directories with null contents and directory stats"]),
    6: (__test_7372635f746573742e6d6274_6, ["src filters files changed before since"]),
    7: (__test_7372635f746573742e6d6274_7, ["src includes files with ctime after since"]),
    8: (__test_7372635f746573742e6d6274_8, ["src can expose file contents as byte streams"]),
    9: (__test_7372635f746573742e6d6274_9, ["src strips utf8 bom from buffered files by default"]),
    10: (__test_7372635f746573742e6d6274_10, ["src preserves utf8 bom when removeBOM is false"]),
    11: (__test_7372635f746573742e6d6274_11, ["src preserves raw bytes when encoding is false"]),
    12: (__test_7372635f746573742e6d6274_12, ["src attaches external sourcemap files when sourcemaps is true"]),
    13: (__test_7372635f746573742e6d6274_13, ["src resolved symlinks expose target file metadata"]),
    14: (__test_7372635f746573742e6d6274_14, ["src can preserve symlinks when resolve symlinks is false"]),
  },
  "src.mbt": {
  },
  "sourcemap_test.mbt": {
    0: (__test_736f757263656d61705f746573742e6d6274_0, ["vinyl-fs sourcemap attaches source maps to vinyl files"]),
    1: (__test_736f757263656d61705f746573742e6d6274_1, ["vinyl-fs sourcemap creates external map vinyl files"]),
    2: (__test_736f757263656d61705f746573742e6d6274_2, ["vinyl-fs sourcemap skips files without source maps"]),
  },
  "sourcemap.mbt": {
  },
  "options_test.mbt": {
    0: (__test_6f7074696f6e735f746573742e6d6274_0, ["vinyl-fs options resolve fixed values"]),
    1: (__test_6f7074696f6e735f746573742e6d6274_1, ["vinyl-fs options resolve values from vinyl files"]),
    2: (__test_6f7074696f6e735f746573742e6d6274_2, ["vinyl-fs options allow defaults before resolving"]),
  },
  "options.mbt": {
  },
  "gulp_recipe_test.mbt": {
    0: (__test_67756c705f7265636970655f746573742e6d6274_0, ["gulp recipe copy rename with through2 vinyl transform"]),
    1: (__test_67756c705f7265636970655f746573742e6d6274_1, ["gulp recipe filter duplicate with through2 vinyl flat transform"]),
  },
  "glob_stream_test.mbt": {
    0: (__test_676c6f625f73747265616d5f746573742e6d6274_0, ["vinyl-fs glob stream preserves include and negation order"]),
    1: (__test_676c6f625f73747265616d5f746573742e6d6274_1, ["vinyl-fs glob stream applies dot option"]),
  },
  "glob_stream.mbt": {
  },
  "file_operations_test.mbt": {
    0: (__test_66696c655f6f7065726174696f6e735f746573742e6d6274_0, ["vinyl-fs file operations read file metadata"]),
    1: (__test_66696c655f6f7065726174696f6e735f746573742e6d6274_1, ["vinyl-fs file operations report missing metadata as errors"]),
    2: (__test_66696c655f6f7065726174696f6e735f746573742e6d6274_2, ["vinyl-fs file operations read symlink targets"]),
  },
  "file_operations.mbt": {
  },
  "dest_test.mbt": {
    0: (__test_646573745f746573742e6d6274_0, ["dest writes files under output directory and returns updated vinyl paths"]),
    1: (__test_646573745f746573742e6d6274_1, ["dest accepts through2 vinyl transforms"]),
    2: (__test_646573745f746573742e6d6274_2, ["dest accepts through2 vinyl transformer flush output"]),
    3: (__test_646573745f746573742e6d6274_3, ["dest writes directories to the output directory"]),
    4: (__test_646573745f746573742e6d6274_4, ["dest writes streaming vinyl contents"]),
    5: (__test_646573745f746573742e6d6274_5, ["dest writes symbolic vinyl files as symlinks"]),
    6: (__test_646573745f746573742e6d6274_6, ["dest preserves existing symlink when overwrite is false"]),
    7: (__test_646573745f746573742e6d6274_7, ["dest errors when symbolic vinyl files do not have symlink targets"]),
    8: (__test_646573745f746573742e6d6274_8, ["dest resolves relative output directory from cwd option"]),
    9: (__test_646573745f746573742e6d6274_9, ["dest applies mode and dirMode to returned vinyl stats"]),
    10: (__test_646573745f746573742e6d6274_10, ["dest applies mode and dirMode to filesystem entries"]),
    11: (__test_646573745f746573742e6d6274_11, ["dest applies mtime to filesystem files"]),
    12: (__test_646573745f746573742e6d6274_12, ["dest preserves existing file when overwrite is false"]),
    13: (__test_646573745f746573742e6d6274_13, ["dest appends file contents when append is true"]),
    14: (__test_646573745f746573742e6d6274_14, ["dest writes external sourcemap files when sourcemaps location is set"]),
    15: (__test_646573745f746573742e6d6274_15, ["dest inlines sourcemap files when inline_sourcemaps is true"]),
    16: (__test_646573745f746573742e6d6274_16, ["dest skips files read with read false"]),
  },
  "dest.mbt": {
  },
  "codecs_test.mbt": {
    0: (__test_636f646563735f746573742e6d6274_0, ["vinyl-fs codecs expose utf8 as the default encoding"]),
    1: (__test_636f646563735f746573742e6d6274_1, ["vinyl-fs codecs decode utf8 and strip bom by default"]),
    2: (__test_636f646563735f746573742e6d6274_2, ["vinyl-fs codecs preserve raw bytes when encoding is false"]),
    3: (__test_636f646563735f746573742e6d6274_3, ["vinyl-fs codecs encode text as utf8 bytes"]),
    4: (__test_636f646563735f746573742e6d6274_4, ["decode_text utf16le decodes little-endian UTF-16 to UTF-8 string"]),
    5: (__test_636f646563735f746573742e6d6274_5, ["decode_text utf16be decodes big-endian UTF-16 to UTF-8 string"]),
    6: (__test_636f646563735f746573742e6d6274_6, ["decode_text utf16le strips BOM"]),
    7: (__test_636f646563735f746573742e6d6274_7, ["encode_text utf16le encodes UTF-8 string to little-endian UTF-16"]),
    8: (__test_636f646563735f746573742e6d6274_8, ["decode_text utf8 (default) still works"]),
    9: (__test_636f646563735f746573742e6d6274_9, ["decode_src_bytes Raw leaves BOM byte intact"]),
    10: (__test_636f646563735f746573742e6d6274_10, ["decode_src_bytes UTF8 strips BOM when remove_bom=true"]),
    11: (__test_636f646563735f746573742e6d6274_11, ["decode_src_bytes UTF8 keeps BOM when remove_bom=false"]),
    12: (__test_636f646563735f746573742e6d6274_12, ["decode_src_bytes Charset utf16le transcodes to utf-8"]),
    13: (__test_636f646563735f746573742e6d6274_13, ["encode_dest_bytes UTF8 is identity"]),
    14: (__test_636f646563735f746573742e6d6274_14, ["encode_dest_bytes Raw is identity"]),
    15: (__test_636f646563735f746573742e6d6274_15, ["encode_dest_bytes Charset utf16le transcodes from utf-8"]),
  },
  "codecs.mbt": {
  },
}


///|
impl MoonBit_Test_Driver for MoonBit_Test_Driver_Internal_No_Args with run_test(
  _,
  filename,
  index,
  handle_result,
  error_to_string,
) {
  if moonbit_test_driver_internal_no_args_tests.0.get(filename)
    is Some(index_map) &&
    index_map.get(index) is Some((f, attrs)) {
    let name = if attrs is [name, ..] { name } else { "" }
    if MOONBIT_TEST_DRIVER_INTERNAL_IS_NATIVE {
      for attr in attrs {
        if attr is [.. "panic", ..] {
          raise MoonBitTestDriverInternalSkipTest(name~)
        }
      }
    }
    moonbit_test_driver_internal_catch_error(
      f,
      on_ok=() => handle_result(name, "", false),
      on_err=err => handle_result(name, error_to_string(err), false),
    )
    true
  } else {
    false
  }
}
///|
#cfg(target="native")
#test_entry
fn main {
  let async_tests = []
  for arg in moonbit_test_driver_internal_native_parse_args() {
    moonbit_test_driver_internal_do_execute(async_tests, arg.0, arg.1)
  }
  MoonBit_Async_Test_Driver_Impl::run_async_tests(async_tests)
}

///|
#cfg(target="js")
#test_entry
fn main {
  let test_params = moonbit_test_driver_js_parse_args()
  moonbit_test_driver_internal_execute(test_params)
  moonbit_test_driver_finish()
}

///|
#cfg(any(target="wasm", target="wasm-gc"))
#test_entry
fn main {
  ()
}

///|
#cfg(target="js")
extern "js" fn moonbit_test_driver_js_parse_args() -> Array[
  MoonBitTestDriverInternalTestEntry,
] =
  #|() => {
  #|  try {
  #|    let testParams = []
  #|    // check if there is command line argument
  #|    if (process.argv.length > 2) {
  #|      const testArgs = JSON.parse(process.argv[2])
  #|      testParams = testArgs.file_and_index.flatMap(([file, ranges]) => {
  #|        let arr = []
  #|        for (let range of ranges) {
  #|          for (let i = range.start; i < range.end; i++) {
  #|            arr.push([file, i])
  #|          }
  #|        }
  #|        return arr
  #|      })
  #|      return testParams
  #|    }
  #|  } catch (error) {
  #|    console.error('failed to parse args: ', error.message)
  #|    process.exit(1)
  #|  }
  #|}

///|
#cfg(target="js")
pub fn moonbit_test_driver_internal_execute(
  tests : Array[MoonBitTestDriverInternalTestEntry],
) -> Unit {
  let async_tests = []
  for i in 0..<tests.length() {
    let entry = tests[i]
    moonbit_test_driver_internal_do_execute(
      async_tests,
      entry.filename(),
      entry.index(),
    )
  }
  MoonBit_Async_Test_Driver_Impl::run_async_tests(async_tests)
}

///|
#cfg(any(target="wasm", target="wasm-gc"))
pub fn moonbit_test_driver_internal_execute(
  filename : MoonbitTestDriverInternalExternString,
  index : Int,
) -> Unit {
  let filename = moonbit_test_driver_internal_get_file_name(filename)
  let async_tests = []
  moonbit_test_driver_internal_do_execute(async_tests, filename, index)
  MoonBit_Async_Test_Driver_Impl::run_async_tests(async_tests)
}

///|
#cfg(not(target="native"))
pub fn moonbit_test_driver_finish() -> Unit {
  
}

///|
fn moonbit_test_driver_internal_do_execute(
  async_tests : Array[async () -> Unit],
  filename : String,
  index : Int,
) -> Unit {
  // These two are internalized to avoid unused warnings

  fn handle_result(testname : String, message : String, skipped : Bool) {
    // In WASM, this function is called by the external test driver, but here
    // in native mode there's nothing that calls it, so we call it manually.
    if !skipped && MOONBIT_TEST_DRIVER_INTERNAL_IS_NATIVE {
      
    }
    let file_name = filename.escape()
    let test_name = testname.escape()
    let message = message.escape()
    @moonbitlang/core/prelude.println("----- BEGIN MOON TEST RESULT -----")
    @moonbitlang/core/prelude.println(
      "{\"package\": \"mulpjs/mulp/vinyl_fs\", \"filename\": \{file_name}, \"index\": \"\{index}\", \"test_name\": \{test_name}, \"message\": \{message}}",
    )
    @moonbitlang/core/prelude.println("----- END MOON TEST RESULT -----")
  }

  fn error_to_string(err : Error) -> String {
    match err {
      @moonbitlang/core/prelude.Failure::Failure(e)
      | @moonbitlang/core/prelude.InspectError::InspectError(e)
      | @moonbitlang/core/prelude.SnapshotError::SnapshotError(e)
      | MoonBitTestDriverInternalJsError(e) => e
      e => moonbit_test_driver_internal_error_to_string(e)
    }
  }

  try {
    if !(MoonBit_Test_Driver_Internal_No_Args::run_test(
        async_tests, filename, index, handle_result, error_to_string,
      ) ||
      MoonBit_Test_Driver_Internal_With_Args::run_test(
        async_tests, filename, index, handle_result, error_to_string,
      ) ||
      MoonBit_Test_Driver_Internal_Async_No_Args::run_test(
        async_tests, filename, index, handle_result, error_to_string,
      ) ||
      MoonBit_Test_Driver_Internal_Async_With_Args::run_test(
        async_tests, filename, index, handle_result, error_to_string,
      ) ||
      MoonBit_Test_Driver_Internal_With_Bench_Args::run_test(
        async_tests, filename, index, handle_result, error_to_string,
      )) {
      raise MoonBitTestDriverInternalSkipTest(name="")
    }
  } catch {
    err => {
      guard err is MoonBitTestDriverInternalSkipTest(name~)
      handle_result(name, "skipped test", true)
    }
  }
}
