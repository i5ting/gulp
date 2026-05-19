///|
pub(all) enum PlatformKind {
  MacOS
  Linux
  Windows
}

///|
fn upper_ascii_letter(value : String) -> String {
  match value {
    "a" => "A"
    "b" => "B"
    "c" => "C"
    "d" => "D"
    "e" => "E"
    "f" => "F"
    "g" => "G"
    "h" => "H"
    "i" => "I"
    "j" => "J"
    "k" => "K"
    "l" => "L"
    "m" => "M"
    "n" => "N"
    "o" => "O"
    "p" => "P"
    "q" => "Q"
    "r" => "R"
    "s" => "S"
    "t" => "T"
    "u" => "U"
    "v" => "V"
    "w" => "W"
    "x" => "X"
    "y" => "Y"
    "z" => "Z"
    _ => value
  }
}

///|
fn lower_ascii_letter(value : String) -> String {
  match value {
    "A" => "a"
    "B" => "b"
    "C" => "c"
    "D" => "d"
    "E" => "e"
    "F" => "f"
    "G" => "g"
    "H" => "h"
    "I" => "i"
    "J" => "j"
    "K" => "k"
    "L" => "l"
    "M" => "m"
    "N" => "n"
    "O" => "o"
    "P" => "p"
    "Q" => "q"
    "R" => "r"
    "S" => "s"
    "T" => "t"
    "U" => "u"
    "V" => "v"
    "W" => "w"
    "X" => "x"
    "Y" => "y"
    "Z" => "z"
    _ => value
  }
}

///|
fn lower_ascii(value : String) -> String {
  let parts : Array[String] = []
  for index in 0..<value.length() {
    parts.push(lower_ascii_letter(value[index:index + 1].to_owned()))
  }
  parts.join("")
}

///|
fn slash_separated(path : String) -> String {
  let parts : Array[String] = []
  for part in path.split("\\") {
    parts.push(part.to_owned())
  }
  parts.join("/")
}

///|
fn collapse_slashes(path : String, leading~ : Bool) -> String {
  let parts : Array[String] = []
  for part in path.split("/") {
    let value = part.to_owned()
    if value != "" {
      parts.push(value)
    }
  }
  let joined = parts.join("/")
  if leading {
    if joined == "" {
      "/"
    } else {
      "/" + joined
    }
  } else {
    joined
  }
}

///|
pub fn normalize_platform_path(
  platform : PlatformKind,
  path : String,
) -> String {
  match platform {
    Windows => {
      let raw = slash_separated(path)
      if raw.length() >= 2 && raw[1:2].to_owned() == ":" {
        let drive = upper_ascii_letter(raw[0:1].to_owned()) + ":"
        let rest = raw[2:].to_owned()
        let collapsed = collapse_slashes(rest, leading=false)
        if collapsed == "" {
          drive + "/"
        } else {
          drive + "/" + collapsed
        }
      } else {
        collapse_slashes(raw, leading=raw.has_prefix("/"))
      }
    }
    MacOS | Linux => collapse_slashes(path, leading=path.has_prefix("/"))
  }
}

///|
pub fn paths_equal(
  platform : PlatformKind,
  left : String,
  right : String,
) -> Bool {
  let left = normalize_platform_path(platform, left)
  let right = normalize_platform_path(platform, right)
  match platform {
    Linux => left == right
    MacOS | Windows => lower_ascii(left) == lower_ascii(right)
  }
}
