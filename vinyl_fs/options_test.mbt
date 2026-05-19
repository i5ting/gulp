///|
fn option_test_file() -> @vinyl.VinylFile {
  @vinyl.vinyl_file(
    cwd="/workspace",
    base="/workspace/src",
    path=Some("/workspace/src/app.txt"),
    metadata=[("mode", "420")],
  )
}

///|
test "vinyl-fs options resolve fixed values" {
  let file = option_test_file()
  inspect(fixed_option("/tmp/out").resolve(file), content="/tmp/out")
  inspect(fixed_option(0o755).resolve(file), content="493")
  inspect(fixed_option(true).resolve(file), content="true")
}

///|
test "vinyl-fs options resolve values from vinyl files" {
  let file = option_test_file()
  let out_dir = file_option(fn(input : @vinyl.VinylFile) -> String {
    "/tmp/" + input.stem()
  })
  let mode = file_option(fn(input : @vinyl.VinylFile) -> Int {
    match input.metadata_value("mode") {
      Some("420") => 0o644
      _ => 0o600
    }
  })
  inspect(out_dir.resolve(file), content="/tmp/app")
  inspect(mode.resolve(file), content="420")
}

///|
test "vinyl-fs options allow defaults before resolving" {
  let file = option_test_file()
  let missing : VinylOption[String]? = None
  let present : VinylOption[String]? = Some(fixed_option("custom"))
  inspect(resolve_option_or(missing, file, "fallback"), content="fallback")
  inspect(resolve_option_or(present, file, "fallback"), content="custom")
}
