// DOT_RE equivalent: /\..*\.(sw[px])$|~$|\.subl.*\.tmp/
fn is_dot_swap_file(path : String) -> Bool {
  let name = path_basename(path)
  if name.has_suffix("~") {
    return true
  }
  if (name.has_suffix(".swp") || name.has_suffix(".swx")) && name.has_prefix(".") {
    return true
  }
  let subl_idx = name.find(".subl")
  if subl_idx is Some(_) && name.has_suffix(".tmp") {
    return true
  }
  false
}

///|
fn recursive_path_matches(root : String, path : String) -> Bool {
  let root = normalize_path(root)
  let path = normalize_path(path)
  root == "/" || path == root || path.has_prefix(root + "/")
}

///|
fn matcher_matches(
  path : String,
  stats : FileStats?,
  matcher : Matcher,
) -> Bool {
  match matcher {
    MatchPath(target) => normalize_path(target) == normalize_path(path)
    MatchRecursivePath(target) => recursive_path_matches(target, path)
    MatchFunction(fn_match) => fn_match(path, stats)
    MatchRegex(re) =>
      match re.match_(path) {
        Some(result) => result.matched()
        None => false
      }
  }
}

///|
fn is_ignored(
  path : String,
  stats~ : FileStats?,
  ignored : Array[Matcher],
) -> Bool {
  ignored.any(matcher => matcher_matches(path, stats, matcher))
}
