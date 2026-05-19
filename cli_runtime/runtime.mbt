///|
pub struct CliRuntime {
  cwd : String
  discovered : String?
  explicit : String?
}

///|
pub fn cli_runtime(
  cwd~ : String,
  discovered? : String? = None,
  explicit? : String? = None,
) -> CliRuntime {
  { cwd, discovered, explicit }
}

///|
pub fn CliRuntime::cwd(self : CliRuntime) -> String {
  self.cwd
}

///|
pub fn CliRuntime::entry_file(self : CliRuntime) -> String? {
  match self.explicit {
    Some(path) => Some(path)
    None => self.discovered
  }
}

///|
pub fn cli_help(version : String) -> String {
  "mulp " + version + "\nUsage: mulp [--tasks|--tree|--watch] [task]"
}

///|
fn known_flags() -> Array[String] {
  [
    "-v",
    "-T",
    "--help",
    "--version",
    "--cwd",
    "--mulpfile",
    "--gulpfile",
    "--file",
    "--tasks",
    "--tasks-simple",
    "--tree",
    "--watch",
  ]
}

///|
pub fn cli_completions(prefix : String) -> Array[String] {
  let matches : Array[String] = []
  for flag in known_flags() {
    let same_flag_style = if prefix == "-" {
      flag.has_prefix("-") && !flag.has_prefix("--")
    } else {
      flag.has_prefix(prefix)
    }
    if same_flag_style {
      matches.push(flag)
    }
  }
  matches
}

///|
fn version_parts(version : String) -> Array[String] {
  let parts : Array[String] = []
  for part in version.split(".") {
    parts.push(part.to_owned())
  }
  parts
}

///|
fn major_version(version : String) -> String {
  let parts = version_parts(version)
  if parts.length() >= 1 {
    parts[0]
  } else {
    version
  }
}

///|
fn minor_version(version : String) -> String {
  let parts = version_parts(version)
  if parts.length() >= 2 {
    parts[0] + "." + parts[1]
  } else {
    version
  }
}

///|
fn version_satisfies(version : String, range : String) -> Bool {
  if range.has_prefix("^0.") {
    minor_version(version) == range[1:].to_owned()
  } else if range.has_prefix("^") {
    major_version(version) == range[1:].to_owned()
  } else {
    version == range
  }
}

///|
pub fn select_compatible_version(
  versions : Array[String],
  range : String,
) -> String? {
  for version in versions {
    if version_satisfies(version, range) {
      return Some(version)
    }
  }
  None
}

///|
pub fn compatibility_hint(
  requested~ : String,
  available~ : Array[String],
) -> String {
  "No local mulp version satisfies " +
  requested +
  "; available: " +
  available.join(", ")
}
