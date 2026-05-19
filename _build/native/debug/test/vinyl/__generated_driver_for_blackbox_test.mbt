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
  "file_test.mbt": {
    0: (__test_66696c655f746573742e6d6274_0, ["vinyl-master isVinyl returns true for a Vinyl object"]),
    1: (__test_66696c655f746573742e6d6274_1, ["vinyl-master defaults cwd to process cwd"]),
    2: (__test_66696c655f746573742e6d6274_2, ["vinyl-master defaults base to process cwd"]),
    3: (__test_66696c655f746573742e6d6274_3, ["vinyl-master defaults base to cwd property"]),
    4: (__test_66696c655f746573742e6d6274_4, ["vinyl-master defaults path to null"]),
    5: (__test_66696c655f746573742e6d6274_5, ["vinyl-master defaults history to an empty array"]),
    6: (__test_66696c655f746573742e6d6274_6, ["vinyl-master defaults stat to null"]),
    7: (__test_66696c655f746573742e6d6274_7, ["vinyl-master defaults contents to null"]),
    8: (__test_66696c655f746573742e6d6274_8, ["vinyl-master constructor sets base"]),
    9: (__test_66696c655f746573742e6d6274_9, ["vinyl-master constructor sets cwd"]),
    10: (__test_66696c655f746573742e6d6274_10, ["vinyl-master constructor sets path and history"]),
    11: (__test_66696c655f746573742e6d6274_11, ["vinyl-master constructor sets history and path"]),
    12: (__test_66696c655f746573742e6d6274_12, ["vinyl-master constructor sets stat"]),
    13: (__test_66696c655f746573742e6d6274_13, ["vinyl-master constructor sets contents"]),
    14: (__test_66696c655f746573742e6d6274_14, ["vinyl-master constructor sets custom properties as metadata"]),
    15: (__test_66696c655f746573742e6d6274_15, ["vinyl-master constructor sets sourceMap as a first class field"]),
    16: (__test_66696c655f746573742e6d6274_16, ["vinyl-master isCustomProp matches built in field names"]),
    17: (__test_66696c655f746573742e6d6274_17, ["vinyl-master constructor normalizes path"]),
    18: (__test_66696c655f746573742e6d6274_18, ["vinyl-master constructor normalizes and removes trailing separator from path"]),
    19: (__test_66696c655f746573742e6d6274_19, ["vinyl-master constructor normalizes history"]),
    20: (__test_66696c655f746573742e6d6274_20, ["vinyl-master constructor normalizes and removes trailing separator from history"]),
    21: (__test_66696c655f746573742e6d6274_21, ["vinyl-master constructor appends path to history if both exist and different from last"]),
    22: (__test_66696c655f746573742e6d6274_22, ["vinyl-master constructor does not append path to history if both exist and same as last"]),
    23: (__test_66696c655f746573742e6d6274_23, ["vinyl-master constructor does not mutate history array passed in"]),
    24: (__test_66696c655f746573742e6d6274_24, ["vinyl-master isBuffer returns true when contents are a Buffer"]),
    25: (__test_66696c655f746573742e6d6274_25, ["vinyl-master isBuffer returns false when contents are a Stream"]),
    26: (__test_66696c655f746573742e6d6274_26, ["vinyl-master isBuffer returns false when contents are null"]),
    27: (__test_66696c655f746573742e6d6274_27, ["vinyl-master isStream returns false when contents are a Buffer"]),
    28: (__test_66696c655f746573742e6d6274_28, ["vinyl-master isStream returns true when contents are a Stream"]),
    29: (__test_66696c655f746573742e6d6274_29, ["vinyl-master isStream returns false when contents are null"]),
    30: (__test_66696c655f746573742e6d6274_30, ["vinyl-master isNull returns false when contents are a Buffer"]),
    31: (__test_66696c655f746573742e6d6274_31, ["vinyl-master isNull returns false when contents are a Stream"]),
    32: (__test_66696c655f746573742e6d6274_32, ["vinyl-master isNull returns true when contents are null"]),
    33: (__test_66696c655f746573742e6d6274_33, ["vinyl-master isDirectory returns false when contents are a Buffer"]),
    34: (__test_66696c655f746573742e6d6274_34, ["vinyl-master isDirectory returns false when contents are a Stream"]),
    35: (__test_66696c655f746573742e6d6274_35, ["vinyl-master isDirectory returns true when contents are null and stat is directory"]),
    36: (__test_66696c655f746573742e6d6274_36, ["vinyl-master isDirectory returns false when stat exists but is not directory"]),
    37: (__test_66696c655f746573742e6d6274_37, ["vinyl-master isDirectory returns false when stat does not exist"]),
    38: (__test_66696c655f746573742e6d6274_38, ["vinyl-master isSymbolic returns false when contents are a Buffer"]),
    39: (__test_66696c655f746573742e6d6274_39, ["vinyl-master isSymbolic returns false when contents are a Stream"]),
    40: (__test_66696c655f746573742e6d6274_40, ["vinyl-master isSymbolic returns true when contents are null and stat is symbolic link"]),
    41: (__test_66696c655f746573742e6d6274_41, ["vinyl-master isSymbolic returns false when stat exists but is not symbolic link"]),
    42: (__test_66696c655f746573742e6d6274_42, ["vinyl-master isSymbolic returns false when stat does not exist"]),
    43: (__test_66696c655f746573742e6d6274_43, ["vinyl-master clone copies all attributes over with Buffer contents"]),
    44: (__test_66696c655f746573742e6d6274_44, ["vinyl-master clone assigns Buffer content reference when contents option is false"]),
    45: (__test_66696c655f746573742e6d6274_45, ["vinyl-master clone copies all attributes over with Stream contents"]),
    46: (__test_66696c655f746573742e6d6274_46, ["vinyl-master clone does not lose data when clones flow differently data"]),
    47: (__test_66696c655f746573742e6d6274_47, ["vinyl-master clone does not lose data when clones flow differently readable and pipe"]),
    48: (__test_66696c655f746573742e6d6274_48, ["vinyl-master clone contents false still tees stream contents"]),
    49: (__test_66696c655f746573742e6d6274_49, ["vinyl-master clone fixes file symlink if file is a symbolic link"]),
    50: (__test_66696c655f746573742e6d6274_50, ["vinyl-master clone copies all attributes over with null contents"]),
    51: (__test_66696c655f746573742e6d6274_51, ["vinyl-master clone properly clones the stat property"]),
    52: (__test_66696c655f746573742e6d6274_52, ["vinyl-master clone properly clones the history property"]),
    53: (__test_66696c655f746573742e6d6274_53, ["vinyl-master clone copies custom properties as metadata"]),
    54: (__test_66696c655f746573742e6d6274_54, ["vinyl-master clone copies sourceMap"]),
    55: (__test_66696c655f746573742e6d6274_55, ["vinyl-master clone copies history"]),
    56: (__test_66696c655f746573742e6d6274_56, ["vinyl-master clone supports deep and shallow copy of all attributes"]),
    57: (__test_66696c655f746573742e6d6274_57, ["vinyl-master inspect returns correct format when no contents and no path"]),
    58: (__test_66696c655f746573742e6d6274_58, ["vinyl-master inspect returns correct format when Buffer contents and no path"]),
    59: (__test_66696c655f746573742e6d6274_59, ["vinyl-master inspect returns correct format when Buffer contents and relative path"]),
    60: (__test_66696c655f746573742e6d6274_60, ["vinyl-master inspect returns correct format when Stream contents and relative path"]),
    61: (__test_66696c655f746573742e6d6274_61, ["vinyl-master inspect returns correct format when null contents and relative path"]),
    62: (__test_66696c655f746573742e6d6274_62, ["vinyl-master contents setter sets a Buffer"]),
    63: (__test_66696c655f746573742e6d6274_63, ["vinyl-master contents setter sets null"]),
    64: (__test_66696c655f746573742e6d6274_64, ["vinyl-master cwd setter sets cwd"]),
    65: (__test_66696c655f746573742e6d6274_65, ["vinyl-master cwd setter normalizes and removes trailing separator on set"]),
    66: (__test_66696c655f746573742e6d6274_66, ["vinyl-master base getter proxies cwd when omitted"]),
    67: (__test_66696c655f746573742e6d6274_67, ["vinyl-master base getter proxies cwd when same"]),
    68: (__test_66696c655f746573742e6d6274_68, ["vinyl-master base setter proxies to cwd when set to same value"]),
    69: (__test_66696c655f746573742e6d6274_69, ["vinyl-master base setter proxies to cwd when cleared"]),
    70: (__test_66696c655f746573742e6d6274_70, ["vinyl-master base setter sets base"]),
    71: (__test_66696c655f746573742e6d6274_71, ["vinyl-master base setter normalizes and removes trailing separator on set"]),
    72: (__test_66696c655f746573742e6d6274_72, ["vinyl-master relative setter is not represented by immutable API"]),
    73: (__test_66696c655f746573742e6d6274_73, ["vinyl-master relative getter throws on get with no path"]),
    74: (__test_66696c655f746573742e6d6274_74, ["vinyl-master relative returns a relative path from base"]),
    75: (__test_66696c655f746573742e6d6274_75, ["vinyl-master relative returns a relative path from cwd"]),
    76: (__test_66696c655f746573742e6d6274_76, ["vinyl-master relative does not append separator when directory"]),
    77: (__test_66696c655f746573742e6d6274_77, ["vinyl-master relative does not append separator when symlink"]),
    78: (__test_66696c655f746573742e6d6274_78, ["vinyl-master dirname getter throws on get with no path"]),
    79: (__test_66696c655f746573742e6d6274_79, ["vinyl-master dirname returns the dirname without trailing separator"]),
    80: (__test_66696c655f746573742e6d6274_80, ["vinyl-master dirname setter throws on set with no path"]),
    81: (__test_66696c655f746573742e6d6274_81, ["vinyl-master dirname setter replaces the dirname of the path"]),
    82: (__test_66696c655f746573742e6d6274_82, ["vinyl-master basename getter throws on get with no path"]),
    83: (__test_66696c655f746573742e6d6274_83, ["vinyl-master basename returns the basename of the path"]),
    84: (__test_66696c655f746573742e6d6274_84, ["vinyl-master basename does not append trailing separator when directory"]),
    85: (__test_66696c655f746573742e6d6274_85, ["vinyl-master basename does not append trailing separator when symlink"]),
    86: (__test_66696c655f746573742e6d6274_86, ["vinyl-master basename removes trailing separator"]),
    87: (__test_66696c655f746573742e6d6274_87, ["vinyl-master basename setter throws on set with no path"]),
    88: (__test_66696c655f746573742e6d6274_88, ["vinyl-master basename setter replaces the basename of the path"]),
    89: (__test_66696c655f746573742e6d6274_89, ["vinyl-master stem getter throws on get with no path"]),
    90: (__test_66696c655f746573742e6d6274_90, ["vinyl-master stem returns the stem of the path"]),
    91: (__test_66696c655f746573742e6d6274_91, ["vinyl-master stem setter throws on set with no path"]),
    92: (__test_66696c655f746573742e6d6274_92, ["vinyl-master stem setter replaces the stem of the path"]),
    93: (__test_66696c655f746573742e6d6274_93, ["vinyl-master extname getter throws on get with no path"]),
    94: (__test_66696c655f746573742e6d6274_94, ["vinyl-master extname returns the extname of the path"]),
    95: (__test_66696c655f746573742e6d6274_95, ["vinyl-master extname setter throws on set with no path"]),
    96: (__test_66696c655f746573742e6d6274_96, ["vinyl-master extname setter replaces the extname of the path"]),
    97: (__test_66696c655f746573742e6d6274_97, ["vinyl-master path records path in history upon instantiation"]),
    98: (__test_66696c655f746573742e6d6274_98, ["vinyl-master path records path in history when set"]),
    99: (__test_66696c655f746573742e6d6274_99, ["vinyl-master path does not record path in history when set to the current path"]),
    100: (__test_66696c655f746573742e6d6274_100, ["vinyl-master path does not record path in history when set to empty string"]),
    101: (__test_66696c655f746573742e6d6274_101, ["vinyl-master path normalizes the path upon set"]),
    102: (__test_66696c655f746573742e6d6274_102, ["vinyl-master path removes the trailing separator upon set"]),
    103: (__test_66696c655f746573742e6d6274_103, ["vinyl-master symlink returns null on get with no symlink"]),
    104: (__test_66696c655f746573742e6d6274_104, ["vinyl-master symlink setter sets symlink"]),
    105: (__test_66696c655f746573742e6d6274_105, ["vinyl-master symlink setter allows relative symlink"]),
    106: (__test_66696c655f746573742e6d6274_106, ["vinyl-master symlink setter normalizes and removes trailing separator upon set"]),
  },
  "file_bench_test.mbt": {
  },
  "file.mbt": {
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
      "{\"package\": \"mulpjs/mulp/vinyl\", \"filename\": \{file_name}, \"index\": \"\{index}\", \"test_name\": \{test_name}, \"message\": \{message}}",
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
