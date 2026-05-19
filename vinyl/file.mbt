///|
pub(all) enum VinylContents {
  Null
  Buffer(Bytes)
  Stream(@stream.ByteStream)
}

///|
pub(all) enum VinylKind {
  File
  Directory
  SymbolicLink
}

///|
pub struct VinylStat {
  kind : VinylKind
  mode : Int?
  mtime_ms : Int64?
}

///|
pub struct VinylFile {
  cwd : String
  base : String
  history : Array[String]
  contents : VinylContents
  stream_replay : ReplayableStream?
  stat : VinylStat?
  symlink : String?
  source_map : String?
  metadata : Array[(String, String)]
  mut relative_cache : String?
}

///|
struct ReplayableStream {
  source : @stream.ByteStream
  cache : Array[Bytes]
  mut ended : Bool
  mut error : @core.MulpError?
}

///|
fn replayable_stream(source : @stream.ByteStream) -> ReplayableStream {
  { source, cache: [], ended: false, error: None }
}

///|
fn ReplayableStream::reader(self : ReplayableStream) -> @stream.ByteStream {
  let mut index = 0
  @stream.byte_stream_from_pull(fn() -> Result[Bytes?, @core.MulpError] {
    if index < self.cache.length() {
      let chunk = self.cache[index]
      index = index + 1
      Ok(Some(chunk))
    } else if self.ended {
      Ok(None)
    } else {
      match self.error {
        Some(err) => Err(err)
        None =>
          match self.source.read_next() {
            Ok(Some(chunk)) => {
              self.cache.push(chunk)
              index = index + 1
              Ok(Some(chunk))
            }
            Ok(None) => {
              self.ended = true
              Ok(None)
            }
            Err(err) => {
              self.error = Some(err)
              Err(err)
            }
          }
      }
    }
  })
}

///|
fn prepare_contents(
  contents : VinylContents,
) -> (VinylContents, ReplayableStream?) {
  match contents {
    Stream(source) => {
      let replay = replayable_stream(source)
      (Stream(replay.reader()), Some(replay))
    }
    other => (other, None)
  }
}

///|
pub fn vinyl_stat(
  kind~ : VinylKind,
  mode? : Int? = None,
  mtime_ms? : Int64? = None,
) -> VinylStat {
  { kind, mode, mtime_ms }
}

///|
pub fn VinylStat::kind(self : VinylStat) -> VinylKind {
  self.kind
}

///|
pub fn VinylStat::mode(self : VinylStat) -> Int? {
  self.mode
}

///|
pub fn VinylStat::mtime_ms(self : VinylStat) -> Int64? {
  self.mtime_ms
}

///|
fn copy_strings(values : Array[String]) -> Array[String] {
  let copied : Array[String] = []
  for value in values {
    copied.push(value)
  }
  copied
}

///|
fn copy_metadata(values : Array[(String, String)]) -> Array[(String, String)] {
  let copied : Array[(String, String)] = []
  for value in values {
    copied.push(value)
  }
  copied
}

///|
fn trim_trailing_slash(value : String) -> String {
  if value.length() > 1 && value.has_suffix("/") {
    value[:value.length() - 1].to_owned()
  } else {
    value
  }
}

///|
fn path_needs_normalize(value : String) -> Bool {
  let mut segment_start = 0
  let mut index = 0
  while index <= value.length() {
    if index == value.length() || value[index:index + 1] == "/" {
      if index == segment_start {
        if index == 0 && value.has_prefix("/") {
          segment_start = 1
          index = index + 1
          continue
        } else {
          return true
        }
      }
      let segment = value[segment_start:index]
      if segment == "." || segment == ".." {
        return true
      }
      segment_start = index + 1
    }
    index = index + 1
  }
  false
}

///|
fn normalize_path(value : String) -> String {
  if value == "" {
    return ""
  }
  let trimmed = trim_trailing_slash(value)
  if !path_needs_normalize(trimmed) {
    return trimmed
  }
  let absolute = value.has_prefix("/")
  let parts : Array[String] = []
  for raw in value.split("/") {
    let part = raw.to_owned()
    if part == "" || part == "." {
      continue
    } else if part == ".." {
      if parts.length() > 0 && parts[parts.length() - 1] != ".." {
        ignore(parts.remove(parts.length() - 1))
      } else if !absolute {
        parts.push(part)
      }
    } else {
      parts.push(part)
    }
  }
  let joined = parts.join("/")
  if absolute {
    if joined == "" {
      "/"
    } else {
      "/" + joined
    }
  } else if joined == "" {
    "."
  } else {
    trim_trailing_slash(joined)
  }
}

///|
fn append_path(history : Array[String], path : String) -> Array[String] {
  let copied = copy_strings(history)
  let normalized = normalize_path(path)
  if normalized == "" {
    return copied
  }
  let last = if copied.length() == 0 {
    None
  } else {
    Some(copied[copied.length() - 1])
  }
  if last != Some(normalized) {
    copied.push(normalized)
  }
  copied
}

///|
pub fn vinyl_file(
  cwd? : String = default_cwd(),
  base? : String = cwd,
  path? : String? = None,
  history? : Array[String] = [],
  contents? : VinylContents = Null,
  stat? : VinylStat? = None,
  symlink? : String? = None,
  source_map? : String? = None,
  metadata? : Array[(String, String)] = [],
) -> VinylFile {
  let (prepared_contents, stream_replay) = prepare_contents(contents)
  let normalized_cwd = normalize_path(cwd)
  let normalized_base = normalize_path(base)
  let normalized_history = if history.length() == 0 {
    []
  } else {
    copy_strings(history).map(normalize_path)
  }
  let final_history = match path {
    Some(value) =>
      if normalized_history.length() == 0 {
        let normalized_path = normalize_path(value)
        if normalized_path == "" {
          []
        } else {
          [normalized_path]
        }
      } else {
        append_path(normalized_history, value)
      }
    None => normalized_history
  }
  {
    cwd: normalized_cwd,
    base: normalized_base,
    history: final_history,
    contents: prepared_contents,
    stream_replay,
    stat,
    symlink: symlink.map(normalize_path),
    source_map,
    metadata: copy_metadata(metadata),
    relative_cache: None,
  }
}

///|
pub fn is_vinyl(_file : VinylFile) -> Bool {
  true
}

///|
pub fn is_custom_prop(key : String) -> Bool {
  match key {
    "_contents"
    | "_symlink"
    | "contents"
    | "stat"
    | "history"
    | "path"
    | "_base"
    | "base"
    | "_cwd"
    | "cwd" => false
    _ => true
  }
}

///|
fn default_cwd() -> String {
  @env.current_dir().unwrap_or(".")
}

///|
pub fn VinylFile::cwd(self : VinylFile) -> String {
  self.cwd
}

///|
pub fn VinylFile::base(self : VinylFile) -> String {
  self.base
}

///|
pub fn VinylFile::path(self : VinylFile) -> String? {
  if self.history.length() == 0 {
    None
  } else {
    Some(self.history[self.history.length() - 1])
  }
}

///|
pub fn VinylFile::history(self : VinylFile) -> Array[String] {
  copy_strings(self.history)
}

///|
pub fn VinylFile::contents(self : VinylFile) -> VinylContents {
  self.contents
}

///|
pub fn VinylFile::stat(self : VinylFile) -> VinylStat? {
  self.stat
}

///|
pub fn VinylFile::symlink(self : VinylFile) -> String? {
  self.symlink
}

///|
pub fn VinylFile::source_map(self : VinylFile) -> String? {
  self.source_map
}

///|
pub fn VinylFile::metadata(self : VinylFile) -> Array[(String, String)] {
  copy_metadata(self.metadata)
}

///|
pub fn VinylFile::metadata_value(self : VinylFile, key : String) -> String? {
  for item in self.metadata {
    if item.0 == key {
      return Some(item.1)
    }
  }
  None
}

///|
pub fn VinylFile::relative(self : VinylFile) -> String {
  self.relative_result().unwrap_or("")
}

///|
fn path_parts(path : String) -> Array[String] {
  let parts : Array[String] = []
  for raw in normalize_path(path).split("/") {
    let part = raw.to_owned()
    if part != "" && part != "." {
      parts.push(part)
    }
  }
  parts
}

///|
fn relative_between(base : String, path : String) -> String {
  if path == base {
    return ""
  }
  if base != "/" && path.has_prefix(base + "/") {
    return path[base.length() + 1:].to_owned()
  }
  let base_parts = path_parts(base)
  let path_parts = path_parts(path)
  let mut common = 0
  while common < base_parts.length() &&
        common < path_parts.length() &&
        base_parts[common] == path_parts[common] {
    common = common + 1
  }
  let rel : Array[String] = []
  for _ in common..<base_parts.length() {
    rel.push("..")
  }
  for index in common..<path_parts.length() {
    rel.push(path_parts[index])
  }
  rel.join("/")
}

///|
pub fn VinylFile::relative_result(self : VinylFile) -> Result[String, String] {
  match self.relative_cache {
    Some(value) => Ok(value)
    None =>
      match self.path() {
        None => Err("No path specified! Can not get relative.")
        Some(path) => {
          let value = relative_between(self.base, path)
          self.relative_cache = Some(value)
          Ok(value)
        }
      }
  }
}

///|
fn last_index_of(value : String, needle : String) -> Int? {
  let mut found : Int? = None
  let mut index = 0
  while index < value.length() {
    if value[index:index + 1] == needle {
      found = Some(index)
    }
    index = index + 1
  }
  found
}

///|
pub fn VinylFile::dirname(self : VinylFile) -> String {
  self.dirname_result().unwrap_or(".")
}

///|
pub fn VinylFile::dirname_result(self : VinylFile) -> Result[String, String] {
  match self.path() {
    None => Err("No path specified! Can not get dirname.")
    Some(path) =>
      match last_index_of(path, "/") {
        Some(0) => Ok("/")
        Some(index) => Ok(path[:index].to_owned())
        None => Ok(".")
      }
  }
}

///|
pub fn VinylFile::basename(self : VinylFile) -> String {
  self.basename_result().unwrap_or("")
}

///|
pub fn VinylFile::basename_result(self : VinylFile) -> Result[String, String] {
  match self.path() {
    None => Err("No path specified! Can not get basename.")
    Some(path) =>
      match last_index_of(path, "/") {
        Some(index) => Ok(path[index + 1:].to_owned())
        None => Ok(path)
      }
  }
}

///|
pub fn VinylFile::extname(self : VinylFile) -> String {
  self.extname_result().unwrap_or("")
}

///|
pub fn VinylFile::extname_result(self : VinylFile) -> Result[String, String] {
  if self.path() is None {
    return Err("No path specified! Can not get extname.")
  }
  let name = self.basename()
  match last_index_of(name, ".") {
    Some(0) | None => Ok("")
    Some(index) => Ok(name[index:].to_owned())
  }
}

///|
pub fn VinylFile::stem(self : VinylFile) -> String {
  self.stem_result().unwrap_or("")
}

///|
pub fn VinylFile::stem_result(self : VinylFile) -> Result[String, String] {
  if self.path() is None {
    return Err("No path specified! Can not get stem.")
  }
  let name = self.basename()
  match last_index_of(name, ".") {
    Some(0) | None => Ok(name)
    Some(index) => Ok(name[:index].to_owned())
  }
}

///|
pub fn VinylFile::with_path(self : VinylFile, path : String) -> VinylFile {
  {
    cwd: self.cwd,
    base: self.base,
    history: append_path(self.history, path),
    contents: self.contents,
    stream_replay: self.stream_replay,
    stat: self.stat,
    symlink: self.symlink,
    source_map: self.source_map,
    metadata: copy_metadata(self.metadata),
    relative_cache: None,
  }
}

///|
pub fn VinylFile::with_cwd(self : VinylFile, cwd : String) -> VinylFile {
  let normalized = normalize_path(cwd)
  {
    cwd: normalized,
    base: if self.base == self.cwd {
      normalized
    } else {
      self.base
    },
    history: copy_strings(self.history),
    contents: self.contents,
    stream_replay: self.stream_replay,
    stat: self.stat,
    symlink: self.symlink,
    source_map: self.source_map,
    metadata: copy_metadata(self.metadata),
    relative_cache: None,
  }
}

///|
pub fn VinylFile::with_base(self : VinylFile, base : String) -> VinylFile {
  {
    cwd: self.cwd,
    base: normalize_path(base),
    history: copy_strings(self.history),
    contents: self.contents,
    stream_replay: self.stream_replay,
    stat: self.stat,
    symlink: self.symlink,
    source_map: self.source_map,
    metadata: copy_metadata(self.metadata),
    relative_cache: None,
  }
}

///|
pub fn VinylFile::with_base_default(self : VinylFile) -> VinylFile {
  self.with_base(self.cwd)
}

///|
pub fn VinylFile::with_contents(
  self : VinylFile,
  contents : VinylContents,
) -> VinylFile {
  let (prepared_contents, stream_replay) = prepare_contents(contents)
  {
    cwd: self.cwd,
    base: self.base,
    history: copy_strings(self.history),
    contents: prepared_contents,
    stream_replay,
    stat: self.stat,
    symlink: self.symlink,
    source_map: self.source_map,
    metadata: copy_metadata(self.metadata),
    relative_cache: self.relative_cache,
  }
}

///|
pub fn VinylFile::with_source_map(
  self : VinylFile,
  source_map : String?,
) -> VinylFile {
  {
    cwd: self.cwd,
    base: self.base,
    history: copy_strings(self.history),
    contents: self.contents,
    stream_replay: self.stream_replay,
    stat: self.stat,
    symlink: self.symlink,
    source_map,
    metadata: copy_metadata(self.metadata),
    relative_cache: self.relative_cache,
  }
}

///|
fn join_path(dir : String, name : String) -> String {
  if dir == "/" {
    "/" + name
  } else if dir.has_suffix("/") {
    dir + name
  } else {
    dir + "/" + name
  }
}

///|
pub fn VinylFile::with_dirname(self : VinylFile, dirname : String) -> VinylFile {
  self.with_dirname_result(dirname).unwrap_or(self)
}

///|
pub fn VinylFile::with_dirname_result(
  self : VinylFile,
  dirname : String,
) -> Result[VinylFile, String] {
  match self.basename_result() {
    Ok(name) => Ok(self.with_path(join_path(normalize_path(dirname), name)))
    Err(_) => Err("No path specified! Can not set dirname.")
  }
}

///|
pub fn VinylFile::with_basename(
  self : VinylFile,
  basename : String,
) -> VinylFile {
  self.with_basename_result(basename).unwrap_or(self)
}

///|
pub fn VinylFile::with_basename_result(
  self : VinylFile,
  basename : String,
) -> Result[VinylFile, String] {
  match self.dirname_result() {
    Ok(dir) => Ok(self.with_path(join_path(dir, basename)))
    Err(_) => Err("No path specified! Can not set basename.")
  }
}

///|
pub fn VinylFile::with_stem(self : VinylFile, stem : String) -> VinylFile {
  self.with_stem_result(stem).unwrap_or(self)
}

///|
pub fn VinylFile::with_stem_result(
  self : VinylFile,
  stem : String,
) -> Result[VinylFile, String] {
  match self.extname_result() {
    Ok(extname) => self.with_basename_result(stem + extname)
    Err(_) => Err("No path specified! Can not set stem.")
  }
}

///|
pub fn VinylFile::with_extname(self : VinylFile, extname : String) -> VinylFile {
  self.with_extname_result(extname).unwrap_or(self)
}

///|
pub fn VinylFile::with_extname_result(
  self : VinylFile,
  extname : String,
) -> Result[VinylFile, String] {
  match self.stem_result() {
    Ok(stem) => self.with_basename_result(stem + extname)
    Err(_) => Err("No path specified! Can not set extname.")
  }
}

///|
pub fn VinylFile::with_symlink(self : VinylFile, symlink : String) -> VinylFile {
  {
    cwd: self.cwd,
    base: self.base,
    history: copy_strings(self.history),
    contents: self.contents,
    stream_replay: self.stream_replay,
    stat: self.stat,
    symlink: Some(normalize_path(symlink)),
    source_map: self.source_map,
    metadata: copy_metadata(self.metadata),
    relative_cache: self.relative_cache,
  }
}

///|
pub fn VinylFile::is_buffer(self : VinylFile) -> Bool {
  match self.contents {
    Buffer(_) => true
    _ => false
  }
}

///|
pub fn VinylFile::is_stream(self : VinylFile) -> Bool {
  match self.contents {
    Stream(_) => true
    _ => false
  }
}

///|
pub fn VinylFile::is_null(self : VinylFile) -> Bool {
  match self.contents {
    Null => true
    _ => false
  }
}

///|
pub fn VinylFile::is_directory(self : VinylFile) -> Bool {
  self.is_null() && self.stat is Some({ kind: Directory, .. })
}

///|
pub fn VinylFile::is_symbolic(self : VinylFile) -> Bool {
  self.is_null() && self.stat is Some({ kind: SymbolicLink, .. })
}

///|
pub fn VinylFile::clone(
  self : VinylFile,
  contents? : Bool = true,
  deep? : Bool = true,
) -> VinylFile {
  ignore(deep)
  let cloned_contents = match self.contents {
    Stream(_) =>
      match self.stream_replay {
        Some(replay) => Stream(replay.reader())
        None => self.contents
      }
    Buffer(bytes) if contents => Buffer(bytes)
    other => other
  }
  {
    cwd: self.cwd,
    base: self.base,
    history: copy_strings(self.history),
    contents: cloned_contents,
    stream_replay: self.stream_replay,
    stat: self.stat,
    symlink: self.symlink,
    source_map: self.source_map,
    metadata: copy_metadata(self.metadata),
    relative_cache: self.relative_cache,
  }
}

///|
fn hex_digit(value : Int) -> String {
  match value {
    0 => "0"
    1 => "1"
    2 => "2"
    3 => "3"
    4 => "4"
    5 => "5"
    6 => "6"
    7 => "7"
    8 => "8"
    9 => "9"
    10 => "a"
    11 => "b"
    12 => "c"
    13 => "d"
    14 => "e"
    15 => "f"
    _ => "0"
  }
}

///|
fn byte_to_hex(byte : Byte) -> String {
  let value = byte.to_int()
  hex_digit(value / 16) + hex_digit(value % 16)
}

///|
fn buffer_summary(bytes : Bytes) -> String {
  let parts : Array[String] = []
  for index in 0..<bytes.length() {
    parts.push(byte_to_hex(bytes[index]))
  }
  "<Buffer " + parts.join(" ") + ">"
}

///|
pub fn VinylFile::inspect(self : VinylFile) -> String {
  let parts : Array[String] = []
  match self.path() {
    Some(_) => {
      let rel = self.relative()
      if rel != "" {
        parts.push("\"" + rel + "\"")
      }
    }
    None => ()
  }
  match self.contents {
    Buffer(bytes) => parts.push(buffer_summary(bytes))
    Stream(_) => parts.push("<ReadableStream>")
    Null => ()
  }
  "<File " + parts.join(" ") + ">"
}
