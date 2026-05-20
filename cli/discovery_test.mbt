///|
fn discovery_config_source(entry : String, default_task : String) -> String {
  "[package]\nname = \"site-build\"\n\n[mulp]\nentry = \"" +
  entry +
  "\"\ndefault = \"" +
  default_task +
  "\""
}

///|
test "config document constructor stores path and contents" {
  let document = config_document("/workspace/gulp.mbtx", "contents")
  inspect(document.path, content="/workspace/gulp.mbtx")
  inspect(document.contents, content="contents")
}

///|
test "discover config searches current directory then parents for mulp mbtx" {
  let files = [
    ("/workspace/gulp.mbtx", discovery_config_source("root/build", "root")),
    ("/workspace/app/gulp.mbtx", discovery_config_source("app/build", "app")),
  ]
  let found = discover_config(cwd="/workspace/app/src", files~).unwrap()
  inspect(found.path, content="/workspace/app/gulp.mbtx")
  inspect(parse_config(found.contents).unwrap().mulp.entry, content="app/build")
}

///|
test "discover config does not use legacy mulp mbt when mbtx is missing" {
  let files = [
    (
      "/workspace/app/mulp.mbt",
      discovery_config_source("legacy/build", "legacy"),
    ),
  ]
  let result = discover_config(cwd="/workspace/app", files~)
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Config error: could not find gulp.mbtx from /workspace/app",
  )
}

///|
test "load config uses explicit absolute path" {
  let files = [
    ("/workspace/gulp.mbtx", discovery_config_source("root/build", "root")),
    ("/tmp/custom.toml", discovery_config_source("custom/build", "custom")),
  ]
  let found = load_config(
    cwd="/workspace/app",
    config_path=Some("/tmp/custom.toml"),
    files,
  ).unwrap()
  inspect(found.path, content="/tmp/custom.toml")
  inspect(
    parse_config(found.contents).unwrap().mulp.default_task,
    content="custom",
  )
}

///|
test "load config resolves explicit relative path from cwd" {
  let files = [
    (
      "/workspace/app/config/mulp.toml",
      discovery_config_source("relative/build", "relative"),
    ),
  ]
  let found = load_config(
    cwd="/workspace/app",
    config_path=Some("config/mulp.toml"),
    files,
  ).unwrap()
  inspect(found.path, content="/workspace/app/config/mulp.toml")
}

///|
test "discover config reports missing config" {
  let result = discover_config(cwd="/workspace/app", files=[])
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Config error: could not find gulp.mbtx from /workspace/app",
  )
}

///|
test "discover config does not automatically fall back to mulp toml" {
  let files = [
    (
      "/workspace/app/mulp.toml",
      discovery_config_source("legacy/build", "legacy"),
    ),
  ]
  let result = discover_config(cwd="/workspace/app", files~)
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Config error: could not find gulp.mbtx from /workspace/app",
  )
}
