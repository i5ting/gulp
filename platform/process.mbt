///|
pub struct ProcessCommand {
  program : String
  args : Array[String]
  cwd : String?
}

///|
pub struct ProcessResult {
  exit_code : Int
  stdout : String
  stderr : String
  output : String
}

///|
pub fn process_command(program : String, args : Array[String]) -> ProcessCommand {
  { program, args, cwd: None }
}

///|
pub fn ProcessCommand::with_cwd(
  self : ProcessCommand,
  cwd : String,
) -> ProcessCommand {
  { program: self.program, args: self.args, cwd: Some(cwd) }
}

///|
fn quote_shell_arg(value : String) -> String {
  let parts : Array[String] = []
  for index in 0..<value.length() {
    let char = value[index:index + 1].to_owned()
    if char == "'" {
      parts.push("'\\''")
    } else {
      parts.push(char)
    }
  }
  "'" + parts.join("") + "'"
}

///|
fn ProcessCommand::to_shell_command(self : ProcessCommand) -> String {
  let parts : Array[String] = [quote_shell_arg(self.program)]
  for arg in self.args {
    parts.push(quote_shell_arg(arg))
  }
  let command = parts.join(" ")
  match self.cwd {
    Some(cwd) => "cd " + quote_shell_arg(cwd) + " && " + command
    None => command
  }
}

///|
fn parse_number(value : String) -> Int {
  let mut code = 0
  for index in 0..<value.length() {
    code = code * 10 + (value[index].to_int() - '0'.to_int())
  }
  code
}

///|
fn parse_number_line(encoded : String, start : Int) -> (Int, Int) {
  let mut split_at = encoded.length()
  for index in start..<encoded.length() {
    if encoded[index:index + 1].to_owned() == "\n" {
      split_at = index
      break
    }
  }
  (parse_number(encoded[start:split_at].to_owned()), split_at + 1)
}

///|
fn parse_process_result(encoded : String) -> ProcessResult {
  let (exit_code, after_exit) = parse_number_line(encoded, 0)
  let (stdout_length, after_stdout_length) = parse_number_line(encoded, after_exit)
  let (stderr_length, after_stderr_length) = parse_number_line(
    encoded,
    after_stdout_length,
  )
  let stdout_end = after_stderr_length + stdout_length
  let stderr_end = stdout_end + stderr_length
  let stdout = encoded[after_stderr_length:stdout_end].to_owned()
  let stderr = encoded[stdout_end:stderr_end].to_owned()
  { exit_code, stdout, stderr, output: stdout }
}

///|
fn ensure_trailing_newline(value : String) -> String {
  if value == "" || value.has_suffix("\n") {
    value
  } else {
    value + "\n"
  }
}

///|
pub fn ProcessResult::render_log(self : ProcessResult) -> String {
  let parts : Array[String] = []
  if self.stdout != "" {
    parts.push("[stdout]\n" + ensure_trailing_newline(self.stdout))
  }
  if self.stderr != "" {
    parts.push("[stderr]\n" + ensure_trailing_newline(self.stderr))
  }
  parts.join("")
}

///|
fn bytes_from_ascii(value : String) -> Bytes {
  let bytes : Array[Byte] = []
  for index in 0..<value.length() {
    bytes.push(value[index].to_int().to_byte())
  }
  Bytes::from_array(bytes)
}

///|
fn ascii_from_bytes(value : Bytes) -> String {
  let parts : Array[String] = []
  for index in 0..<value.length() {
    parts.push(value[index].to_int().unsafe_to_char().to_string())
  }
  parts.join("")
}

///|
pub fn run_process(command : ProcessCommand) -> Result[ProcessResult, @core.MulpError] {
  let encoded = ascii_from_bytes(
    run_shell_ffi(bytes_from_ascii(command.to_shell_command())),
  )
  Ok(parse_process_result(encoded))
}

///|
#borrow(command)
extern "c" fn run_shell_ffi(command : Bytes) -> Bytes = "mulp_run_shell"
