///|
test "parse config reads package and mulp sections" {
  let source =
    #|[package]
    #|name = "site-build"
    #|version = "0.1.0"
    #|
    #|[mulp]
    #|entry = "src/build"
    #|default = "build"
  let config = parse_config(source).unwrap()
  inspect(config.pkg.name, content="site-build")
  inspect(config.pkg.version.unwrap(), content="0.1.0")
  inspect(config.mulp.entry, content="src/build")
  inspect(config.mulp.default_task, content="build")
}

///|
test "parse config allows missing package version" {
  let source =
    #|[package]
    #|name = "site-build"
    #|
    #|[mulp]
    #|entry = "src/build"
    #|default = "build"
  let config = parse_config(source).unwrap()
  inspect(config.pkg.version is None, content="true")
}

///|
test "parse config allows missing mulp entry for inferred mulp.mbt packages" {
  let source =
    #|[package]
    #|name = "site-build"
    #|
    #|[mulp]
    #|default = "build"
  let config = parse_config(source).unwrap()
  inspect(config.mulp.entry, content="")
  inspect(config.mulp.default_task, content="build")
}

///|
test "parse config reads minimal mulp mbt hooks" {
  let source =
    #|pub fn package_name() -> String {
    #|  "site-build"
    #|}
    #|
    #|pub fn default_task() -> String {
    #|  "build"
    #|}
  let config = parse_config(source).unwrap()
  inspect(config.pkg.name, content="site-build")
  inspect(config.mulp.entry, content="")
  inspect(config.mulp.default_task, content="build")
}

///|
test "parse config reads optional mulp mbt entry package hook" {
  let source =
    #|pub fn package_name() -> String {
    #|  "site-build"
    #|}
    #|
    #|pub fn entry_package() -> String {
    #|  "src/build"
    #|}
    #|
    #|pub fn default_task() -> String {
    #|  "build"
    #|}
  let config = parse_config(source).unwrap()
  inspect(config.mulp.entry, content="src/build")
  inspect(config.mulp.default_task, content="build")
}

///|
test "parse config rejects unknown top level fields" {
  let source =
    #|name = "site-build"
    #|
    #|[mulp]
    #|entry = "src/build"
    #|default = "build"
  let result = parse_config(source)
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Config error: key outside section: name",
  )
}

///|
test "parse config rejects unquoted values" {
  let source =
    #|[package]
    #|name = site-build
    #|
    #|[mulp]
    #|entry = "src/build"
    #|default = "build"
  let result = parse_config(source)
  inspect(result is Err(_), content="true")
  inspect(
    result.unwrap_err().message(),
    content="Config error: value for package.name must be quoted",
  )
}
