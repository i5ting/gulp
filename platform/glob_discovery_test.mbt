///|
test "glob discovery pulls paths lazily" {
  let paths = ["src/skip.txt", "src/a.mbt", "src/b.mbt"]
  let mut index = 0
  let discovery = discover_glob(fn() -> String? {
    if index >= paths.length() {
      None
    } else {
      let path = paths[index]
      index = index + 1
      Some(path)
    }
  }, "src/**/*.mbt")
  match discovery.next() {
    Some(path) => inspect(path, content="src/a.mbt")
    None => inspect("none", content="src/a.mbt")
  }
  inspect(index, content="2")
  match discovery.next() {
    Some(path) => inspect(path, content="src/b.mbt")
    None => inspect("none", content="src/b.mbt")
  }
  inspect(index, content="3")
  inspect(discovery.next() is None, content="true")
}
