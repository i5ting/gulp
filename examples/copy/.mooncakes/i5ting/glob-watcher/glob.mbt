///| Parse a single glob string (may contain braces) into one or more GlobPatterns.
pub fn GlobPattern::parse(raw : String) -> Array[GlobPattern] {
  expand_braces(raw).map(fn(p) {
    let is_neg = p.has_prefix("!")
    let clean = if is_neg { p[1:].to_owned() } else { p }
    { raw: clean, base_dir: extract_base_dir(clean), is_negation: is_neg }
  })
}

///| Parse an array of glob strings, separating positives from negations.
/// Later positives that match a negation's raw pattern remove that negation.
/// Input array is copied — not mutated.
pub fn parse_globs(
  patterns : Array[String]
) -> (Array[GlobPattern], Array[GlobPattern]) {
  let positives : Array[GlobPattern] = []
  let negations : Array[GlobPattern] = []
  for raw in patterns {
    for gp in GlobPattern::parse(raw) {
      if gp.is_negation {
        negations.push(gp)
      } else {
        positives.push(gp)
      }
    }
  }
  // Re-add resolution: a positive whose raw == a negation's raw cancels it.
  let final_negations = negations.filter(fn(neg) {
    !positives.any(fn(pos) { pos.raw == neg.raw })
  })
  (positives, final_negations)
}

///| Return true if `path` matches the glob `pattern`.
/// Supports: ** * ? [abc] {a,b} and leading ! (treated as literal if already stripped)
pub fn glob_match(pattern : String, path : String) -> Bool {
  expand_braces(pattern).any(fn(p) { glob_match_single(p, path) })
}

// ── internal ──────────────────────────────────────────────────────────────────

fn extract_base_dir(pattern : String) -> String {
  let chars = pattern.to_array()
  let mut last_slash = -1
  for i = 0; i < chars.length(); i = i + 1 {
    let ch = chars[i]
    if ch == '*' || ch == '?' || ch == '[' || ch == '{' {
      if last_slash < 0 {
        return "."
      }
      return pattern[:last_slash].to_owned()
    }
    if ch == '/' {
      last_slash = i
    }
  }
  // No special chars — literal path; return its directory.
  if last_slash < 0 {
    return "."
  }
  pattern[:last_slash].to_owned()
}

fn glob_match_single(pattern : String, path : String) -> Bool {
  let pat = pattern.to_array()
  let pth = path.to_array()
  glob_go(pat, 0, pth, 0)
}

fn glob_go(
  pat : Array[Char],
  pi  : Int,
  pth : Array[Char],
  si  : Int,
) -> Bool {
  let pl = pat.length()
  let sl = pth.length()
  if pi == pl {
    return si == sl
  }
  // ** — match zero or more path segments
  if pi + 1 < pl && pat[pi] == '*' && pat[pi + 1] == '*' {
    let next_pi = if pi + 2 < pl && pat[pi + 2] == '/' {
      pi + 3
    } else {
      pi + 2
    }
    // Try from current position
    if glob_go(pat, next_pi, pth, si) {
      return true
    }
    // Try from each segment boundary
    let mut i = si
    while i < sl {
      if pth[i] == '/' {
        if glob_go(pat, next_pi, pth, i + 1) {
          return true
        }
      }
      i = i + 1
    }
    // ** can consume through the end
    return glob_go(pat, next_pi, pth, sl)
  }
  // * — match any chars within a single segment
  if pat[pi] == '*' {
    let mut i = si
    while i <= sl {
      if glob_go(pat, pi + 1, pth, i) {
        return true
      }
      if i < sl && pth[i] != '/' {
        i = i + 1
      } else {
        break
      }
    }
    return false
  }
  // ? — match exactly one non-separator char
  if pat[pi] == '?' {
    if si < sl && pth[si] != '/' {
      return glob_go(pat, pi + 1, pth, si + 1)
    }
    return false
  }
  // [...] — character class
  if pat[pi] == '[' {
    let (matched, next_pi) = match_char_class(pat, pi + 1, pth, si)
    if matched {
      return glob_go(pat, next_pi, pth, si + 1)
    }
    return false
  }
  // Literal character
  if si < sl && pat[pi] == pth[si] {
    return glob_go(pat, pi + 1, pth, si + 1)
  }
  false
}

fn match_char_class(
  pat : Array[Char],
  pi  : Int,
  pth : Array[Char],
  si  : Int,
) -> (Bool, Int) {
  // Advance pi past ']' regardless of match for next_pi
  let mut i = pi
  let pl = pat.length()
  if si >= pth.length() {
    while i < pl && pat[i] != ']' {
      i = i + 1
    }
    return (false, if i < pl { i + 1 } else { i })
  }
  let ch = pth[si]
  let negate = i < pl && pat[i] == '^'
  if negate {
    i = i + 1
  }
  let mut found = false
  while i < pl && pat[i] != ']' {
    if i + 2 < pl && pat[i + 1] == '-' && pat[i + 2] != ']' {
      if ch >= pat[i] && ch <= pat[i + 2] {
        found = true
      }
      i = i + 3
    } else {
      if ch == pat[i] {
        found = true
      }
      i = i + 1
    }
  }
  let next_pi = if i < pl { i + 1 } else { i }
  (if negate { !found } else { found }, next_pi)
}

fn expand_braces(pattern : String) -> Array[String] {
  match find_brace_range(pattern) {
    None => [pattern]
    Some((start, end_)) => {
      let prefix = pattern[:start].to_owned()
      let inner  = pattern[start + 1:end_].to_owned()
      let suffix = pattern[end_ + 1:].to_owned()
      let result : Array[String] = []
      for alt in split_brace_alts(inner) {
        for expanded in expand_braces(prefix + alt + suffix) {
          result.push(expanded)
        }
      }
      result
    }
  }
}

fn find_brace_range(pattern : String) -> (Int, Int)? {
  let chars = pattern.to_array()
  let mut depth = 0
  let mut start = -1
  for i = 0; i < chars.length(); i = i + 1 {
    match chars[i] {
      '{' => {
        if depth == 0 {
          start = i
        }
        depth = depth + 1
      }
      '}' => {
        depth = depth - 1
        if depth == 0 && start >= 0 {
          return Some((start, i))
        }
      }
      _ => ()
    }
  }
  None
}

fn split_brace_alts(inner : String) -> Array[String] {
  let result : Array[String] = []
  let chars = inner.to_array()
  let mut depth = 0
  let mut start = 0
  for i = 0; i < chars.length(); i = i + 1 {
    match chars[i] {
      '{' => depth = depth + 1
      '}' => depth = depth - 1
      ',' =>
        if depth == 0 {
          result.push(inner[start:i].to_owned())
          start = i + 1
        }
      _ => ()
    }
  }
  result.push(inner[start:].to_owned())
  result
}
