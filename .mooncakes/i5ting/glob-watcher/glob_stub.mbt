// Stub implementations for wasm/wasm-gc/js targets.
// Real implementations are in glob.mbt (native/llvm only).

///|
pub fn GlobPattern::parse(_raw : String) -> Array[GlobPattern] {
  []
}

///|
pub fn parse_globs(
  _patterns : Array[String],
) -> (Array[GlobPattern], Array[GlobPattern]) {
  ([], [])
}

///|
pub fn glob_match(_pattern : String, _path : String) -> Bool {
  false
}
