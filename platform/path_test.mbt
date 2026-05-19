///|
test "path normalization collapses posix separators" {
  inspect(
    normalize_platform_path(Linux, "/workspace//src/"),
    content="/workspace/src",
  )
}

///|
test "path normalization converts windows separators and drive letter" {
  inspect(
    normalize_platform_path(Windows, "c:\\workspace\\\\src\\"),
    content="C:/workspace/src",
  )
}

///|
test "path comparison is case sensitive on linux" {
  inspect(
    paths_equal(Linux, "/workspace/App", "/workspace/app"),
    content="false",
  )
}

///|
test "path comparison is case insensitive on macos and windows" {
  inspect(
    paths_equal(MacOS, "/workspace/App", "/workspace/app"),
    content="true",
  )
  inspect(
    paths_equal(Windows, "c:\\Workspace\\App", "C:/workspace/app"),
    content="true",
  )
}

///|
test "platform path matrix covers macos linux and windows differences" {
  inspect(normalize_platform_path(MacOS, "/Users/me//Project"), content="/Users/me/Project")
  inspect(normalize_platform_path(Linux, "/home/me//Project"), content="/home/me/Project")
  inspect(normalize_platform_path(Windows, "d:\\Project\\src"), content="D:/Project/src")
  inspect(paths_equal(MacOS, "/Users/me/Project", "/Users/me/project"), content="true")
  inspect(paths_equal(Linux, "/home/me/Project", "/home/me/project"), content="false")
  inspect(paths_equal(Windows, "D:\\Project", "d:/project"), content="true")
}
