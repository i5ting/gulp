///|
fn non_empty_segments(path : String) -> Array[String] {
  let segments : Array[String] = []
  for segment in path.split("/") {
    let value = segment.to_owned()
    if value != "" {
      segments.push(value)
    }
  }
  segments
}

///|
fn segment_matches(pattern : String, value : String, pattern_index : Int, value_index : Int) -> Bool {
  if pattern_index == pattern.length() {
    value_index == value.length()
  } else if pattern[pattern_index:pattern_index + 1].to_owned() == "*" {
    segment_matches(pattern, value, pattern_index + 1, value_index) ||
    (value_index < value.length() &&
      segment_matches(pattern, value, pattern_index, value_index + 1))
  } else if value_index < value.length() &&
    pattern[pattern_index:pattern_index + 1] == value[value_index:value_index + 1] {
    segment_matches(pattern, value, pattern_index + 1, value_index + 1)
  } else {
    false
  }
}

///|
fn path_segments_match(
  pattern : Array[String],
  path : Array[String],
  pattern_index : Int,
  path_index : Int,
) -> Bool {
  if pattern_index == pattern.length() {
    path_index == path.length()
  } else if pattern[pattern_index] == "**" {
    path_segments_match(pattern, path, pattern_index + 1, path_index) ||
    (path_index < path.length() &&
      path_segments_match(pattern, path, pattern_index, path_index + 1))
  } else if path_index < path.length() &&
    segment_matches(pattern[pattern_index], path[path_index], 0, 0) {
    path_segments_match(pattern, path, pattern_index + 1, path_index + 1)
  } else {
    false
  }
}

///|
pub fn glob_matches(pattern : String, path : String) -> Bool {
  path_segments_match(non_empty_segments(pattern), non_empty_segments(path), 0, 0)
}

///|
pub fn expand_glob(paths : Array[String], pattern : String) -> Array[String] {
  let matches : Array[String] = []
  for path in paths {
    if glob_matches(pattern, path) {
      matches.push(path)
    }
  }
  matches
}

///|
fn contains_exact_path(paths : Array[String], path : String) -> Bool {
  for item in paths {
    if item == path {
      return true
    }
  }
  false
}

///|
pub fn expand_globs(paths : Array[String], patterns : Array[String]) -> Array[String] {
  let matches : Array[String] = []
  for path in paths {
    let mut included = false
    let mut excluded = false
    for pattern in patterns {
      if pattern.has_prefix("!") {
        let exclude = pattern[1:].to_owned()
        if glob_matches(exclude, path) {
          excluded = true
        }
      } else if glob_matches(pattern, path) {
        included = true
      }
    }
    if included && !excluded && !contains_exact_path(matches, path) {
      matches.push(path)
    }
  }
  matches
}

///|
fn path_has_dot_segment(path : String) -> Bool {
  for segment in non_empty_segments(path) {
    if segment.has_prefix(".") {
      return true
    }
  }
  false
}

///|
pub fn expand_globs_with_options(
  paths : Array[String],
  patterns : Array[String],
  dot~ : Bool,
) -> Array[String] {
  let matches : Array[String] = []
  for path in expand_globs(paths, patterns) {
    if dot || !path_has_dot_segment(path) {
      matches.push(path)
    }
  }
  matches
}

///|
pub fn expand_globs_checked(
  paths : Array[String],
  patterns : Array[String],
  allow_empty~ : Bool,
) -> Result[Array[String], @core.MulpError] {
  let matches = expand_globs(paths, patterns)
  if matches.length() == 0 && !allow_empty {
    Err(@core.glob_error("no files matched: " + patterns.join(",")))
  } else {
    Ok(matches)
  }
}

///|
pub type PullPath = () -> String?

///|
struct GlobDiscovery {
  pull : PullPath
  pattern : String
}

///|
pub fn discover_glob(pull : PullPath, pattern : String) -> GlobDiscovery {
  { pull, pattern }
}

///|
pub fn GlobDiscovery::next(self : GlobDiscovery) -> String? {
  while true {
    match (self.pull)() {
      Some(path) =>
        if glob_matches(self.pattern, path) {
          return Some(path)
        }
      None => return None
    }
  }
  None
}

///|
pub(all) enum GlobWatchEvent {
  Added(String)
  Removed(String)
}

///|
pub fn GlobWatchEvent::render(self : GlobWatchEvent) -> String {
  match self {
    Added(path) => "added:" + path
    Removed(path) => "removed:" + path
  }
}

///|
pub(all) enum WatchEvent {
  Add(String)
  Change(String)
  Unlink(String)
  Ready
  Error(String)
}

///|
pub fn WatchEvent::render(self : WatchEvent) -> String {
  match self {
    Add(path) => "add:" + path
    Change(path) => "change:" + path
    Unlink(path) => "unlink:" + path
    Ready => "ready"
    Error(detail) => "error:" + detail
  }
}

///|
pub fn render_watch_events(events : Array[WatchEvent]) -> String {
  let rendered : Array[String] = []
  for event in events {
    rendered.push(event.render())
  }
  rendered.join(",")
}

///|
pub fn normalize_glob_watch_events(
  events : Array[GlobWatchEvent],
  ready~ : Bool,
) -> Array[WatchEvent] {
  let normalized : Array[WatchEvent] = []
  for event in events {
    match event {
      Added(path) => normalized.push(Add(path))
      Removed(path) => normalized.push(Unlink(path))
    }
  }
  if ready {
    normalized.push(Ready)
  }
  normalized
}

///|
struct GlobWatch {
  platform : PlatformKind
  includes : Array[String]
  excludes : Array[String]
  mut paths : Array[String]
}

///|
fn contains_path_on(platform : PlatformKind, paths : Array[String], path : String) -> Bool {
  for item in paths {
    if paths_equal(platform, item, path) {
      return true
    }
  }
  false
}

///|
fn glob_matches_on(platform : PlatformKind, pattern : String, path : String) -> Bool {
  match platform {
    Linux => glob_matches(pattern, path)
    MacOS | Windows => glob_matches(lower_ascii(pattern), lower_ascii(path))
  }
}

///|
fn path_matches_any_on(
  platform : PlatformKind,
  patterns : Array[String],
  path : String,
) -> Bool {
  for pattern in patterns {
    let pattern = normalize_platform_path(platform, pattern)
    if glob_matches_on(platform, pattern, path) {
      return true
    }
  }
  false
}

///|
fn expand_globs_on(
  platform : PlatformKind,
  paths : Array[String],
  includes : Array[String],
  excludes : Array[String],
) -> Array[String] {
  let matches : Array[String] = []
  for path in paths {
    let path = normalize_platform_path(platform, path)
    if path_matches_any_on(platform, includes, path) &&
      !path_matches_any_on(platform, excludes, path) &&
      !contains_path_on(platform, matches, path) {
      matches.push(path)
    }
  }
  matches
}

///|
pub fn watch_glob_on(
  platform : PlatformKind,
  paths : Array[String],
  pattern : String,
) -> GlobWatch {
  watch_globs_on(platform, paths, includes=[pattern], excludes=[])
}

///|
pub fn watch_globs_on(
  platform : PlatformKind,
  paths : Array[String],
  includes~ : Array[String],
  excludes~ : Array[String],
) -> GlobWatch {
  {
    platform,
    includes,
    excludes,
    paths: expand_globs_on(platform, paths, includes, excludes),
  }
}

///|
pub fn watch_glob(paths : Array[String], pattern : String) -> GlobWatch {
  watch_glob_on(Linux, paths, pattern)
}

///|
pub fn watch_globs(
  paths : Array[String],
  includes~ : Array[String],
  excludes~ : Array[String],
) -> GlobWatch {
  watch_globs_on(Linux, paths, includes~, excludes~)
}

///|
pub fn GlobWatch::poll(
  self : GlobWatch,
  paths : Array[String],
) -> Array[GlobWatchEvent] {
  let next = expand_globs_on(self.platform, paths, self.includes, self.excludes)
  let events : Array[GlobWatchEvent] = []
  for path in next {
    if !contains_path_on(self.platform, self.paths, path) {
      events.push(Added(path))
    }
  }
  for path in self.paths {
    if !contains_path_on(self.platform, next, path) {
      events.push(Removed(path))
    }
  }
  self.paths = next
  events
}
