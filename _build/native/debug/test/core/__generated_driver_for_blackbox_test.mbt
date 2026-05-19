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
  "tree_test.mbt": {
    0: (__test_747265655f746573742e6d6274_0, ["registry tree lists registered tasks"]),
    1: (__test_747265655f746573742e6d6274_1, ["registry tree renders series and parallel composition"]),
    2: (__test_747265655f746573742e6d6274_2, ["tree node public helpers expose name and children"]),
    3: (__test_747265655f746573742e6d6274_3, ["tasks_json renders empty registry"]),
    4: (__test_747265655f746573742e6d6274_4, ["tasks_json renders flat task list"]),
    5: (__test_747265655f746573742e6d6274_5, ["tasks_json renders nested dependencies"]),
  },
  "tree.mbt": {
  },
  "task.mbt": {
  },
  "registry_test.mbt": {
    0: (__test_72656769737472795f746573742e6d6274_0, ["registry registers and finds task by name"]),
    1: (__test_72656769737472795f746573742e6d6274_1, ["custom registry can carry a name"]),
    2: (__test_72656769737472795f746573742e6d6274_2, ["registry reports missing task"]),
    3: (__test_72656769737472795f746573742e6d6274_3, ["registry checked registration rejects duplicate task names"]),
    4: (__test_72656769737472795f746573742e6d6274_4, ["registry init runs a registration callback"]),
    5: (__test_72656769737472795f746573742e6d6274_5, ["registry lists task names in registration order"]),
    6: (__test_72656769737472795f746573742e6d6274_6, ["registry can distinguish exported and private tasks"]),
    7: (__test_72656769737472795f746573742e6d6274_7, ["task can carry display name and description metadata"]),
    8: (__test_72656769737472795f746573742e6d6274_8, ["AsyncRegistry init callback is called when registry is initialised"]),
    9: (__test_72656769737472795f746573742e6d6274_9, ["AsyncRegistry init callback is optional — default registry has no-op init"]),
    10: (__test_72656769737472795f746573742e6d6274_10, ["AsyncRegistry all_tasks returns a copy of registered tasks"]),
    11: (__test_72656769737472795f746573742e6d6274_11, ["AsyncRegistry set_task updates existing task and appends new one"]),
  },
  "registry.mbt": {
  },
  "error_test.mbt": {
    0: (__test_6572726f725f746573742e6d6274_0, ["task not found error renders task name"]),
    1: (__test_6572726f725f746573742e6d6274_1, ["task failed error renders cause"]),
    2: (__test_6572726f725f746573742e6d6274_2, ["config error renders detail"]),
  },
  "error.mbt": {
  },
  "context_test.mbt": {
    0: (__test_636f6e746578745f746573742e6d6274_0, ["context carries cwd and current time"]),
    1: (__test_636f6e746578745f746573742e6d6274_1, ["cancellation token can be requested"]),
  },
  "context.mbt": {
  },
  "compose_test.mbt": {
    0: (__test_636f6d706f73655f746573742e6d6274_0, ["series runs tasks in order"]),
    1: (__test_636f6d706f73655f746573742e6d6274_1, ["task execute supports synchronous task completion"]),
    2: (__test_636f6d706f73655f746573742e6d6274_2, ["series stops on first failure"]),
    3: (__test_636f6d706f73655f746573742e6d6274_3, ["task execute propagates result errors"]),
    4: (__test_636f6d706f73655f746573742e6d6274_4, ["parallel runs every task when all succeed"]),
    5: (__test_636f6d706f73655f746573742e6d6274_5, ["parallel reports first failure and requests cancellation"]),
    6: (__test_636f6d706f73655f746573742e6d6274_6, ["last_run records successful task execution time"]),
    9: (__test_636f6d706f73655f746573742e6d6274_9, ["last_run survives task copies from registry"]),
  },
  "compose.mbt": {
  },
  "async_task_test.mbt": {
    17: (__test_6173796e635f7461736b5f746573742e6d6274_17, ["async registry tasks_json renders flat task list"]),
  },
  "async_task.mbt": {
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
// This file is included if async test is needed
// This also introduces the dependency on async package

///|
#cfg(not(any(target="wasm", target="wasm-gc")))
impl MoonBit_Async_Test_Driver for MoonBit_Async_Test_Driver_Impl with run_async_tests(
  tests,
) {
  let moonbit_test_driver_internal_max_concurrent_tests : Int = 10
  @moonbitlang/async.run_async_main(() => {
    @moonbitlang/async.with_task_group(group => {
      let mut i = 0
      for _ in 0..<moonbit_test_driver_internal_max_concurrent_tests {
        group.spawn_bg(() => {
          while i < tests.length() {
            let f = tests[i]
            i += 1
            f()
          }
        })
      }
    })
    
  })
}

///|
#cfg(not(any(target="wasm", target="wasm-gc")))
impl MoonBit_Async_Test_Driver for MoonBit_Async_Test_Driver_Impl with is_being_cancelled() {
  @moonbitlang/async.is_being_cancelled()
}
// This file is introduced when tests without args is needed but when async is needed
// This also introduces the dependency on async package

///|
let moonbit_test_driver_internal_async_tests : MoonBitTestDriverInternalTestMap[
  async () -> Unit,
] = {
  "tree_test.mbt": {
  },
  "tree.mbt": {
  },
  "task.mbt": {
  },
  "registry_test.mbt": {
  },
  "registry.mbt": {
  },
  "error_test.mbt": {
  },
  "error.mbt": {
  },
  "context_test.mbt": {
  },
  "context.mbt": {
  },
  "compose_test.mbt": {
    7: (__test_636f6d706f73655f746573742e6d6274_7, ["async_parallel_settle runs all tasks even when one fails"]),
    8: (__test_636f6d706f73655f746573742e6d6274_8, ["async_parallel_settle succeeds when all tasks succeed"]),
  },
  "compose.mbt": {
  },
  "async_task_test.mbt": {
    0: (__test_6173796e635f7461736b5f746573742e6d6274_0, ["moonbit async task group runs sleeping tasks concurrently"]),
    1: (__test_6173796e635f7461736b5f746573742e6d6274_1, ["async task executes and records last run"]),
    2: (__test_6173796e635f7461736b5f746573742e6d6274_2, ["sync task can be wrapped as async task"]),
    3: (__test_6173796e635f7461736b5f746573742e6d6274_3, ["async registry registers and requires task by name"]),
    4: (__test_6173796e635f7461736b5f746573742e6d6274_4, ["async registry renders composed task tree"]),
    5: (__test_6173796e635f7461736b5f746573742e6d6274_5, ["async series runs tasks in order"]),
    6: (__test_6173796e635f7461736b5f746573742e6d6274_6, ["async series stops on first failure"]),
    7: (__test_6173796e635f7461736b5f746573742e6d6274_7, ["async parallel starts tasks concurrently"]),
    8: (__test_6173796e635f7461736b5f746573742e6d6274_8, ["async parallel cancels token and aggregates failures"]),
    9: (__test_6173796e635f7461736b5f746573742e6d6274_9, ["callback task completes when callback succeeds"]),
    10: (__test_6173796e635f7461736b5f746573742e6d6274_10, ["callback task can complete asynchronously later"]),
    11: (__test_6173796e635f7461736b5f746573742e6d6274_11, ["callback task propagates callback failure"]),
    12: (__test_6173796e635f7461736b5f746573742e6d6274_12, ["callback task waits when callback is not called"]),
    13: (__test_6173796e635f7461736b5f746573742e6d6274_13, ["callback task rejects duplicate callback completion"]),
    14: (__test_6173796e635f7461736b5f746573742e6d6274_14, ["async task timeout cancels shared token"]),
    15: (__test_6173796e635f7461736b5f746573742e6d6274_15, ["async task timeout wrapper allows fast completion"]),
    16: (__test_6173796e635f7461736b5f746573742e6d6274_16, ["completion protocol rejects stream and callback conflict"]),
  },
  "async_task.mbt": {
  },
}


///|
impl MoonBit_Test_Driver for MoonBit_Test_Driver_Internal_Async_No_Args with run_test(
  async_tests,
  filename,
  index,
  handle_result,
  error_to_string,
) {
  if moonbit_test_driver_internal_async_tests.0.get(filename) is Some(index_map) &&
    index_map.get(index) is Some((f, attrs)) {
    let name = if attrs is [name, ..] { name } else { "" }
    if MOONBIT_TEST_DRIVER_INTERNAL_IS_NATIVE {
      for attr in attrs {
        if attr is [.. "panic", ..] {
          raise MoonBitTestDriverInternalSkipTest(name~)
        }
      }
    }
    async_tests.push(() => {
      try f() catch {
        err if MoonBit_Async_Test_Driver_Impl::is_being_cancelled() => raise err
        err => handle_result(name, error_to_string(err), false)
      } noraise {
        _ => handle_result(name, "", false)
      }
    })
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
      "{\"package\": \"mulpjs/mulp/core\", \"filename\": \{file_name}, \"index\": \"\{index}\", \"test_name\": \{test_name}, \"message\": \{message}}",
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
