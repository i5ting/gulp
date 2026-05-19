///|
test "glob expansion matches direct and nested moonbit files" {
  let paths = [
    "src/main.mbt",
    "src/lib/util.mbt",
    "src/lib/readme.md",
    "test/main.mbt",
  ]
  inspect(expand_glob(paths, "src/**/*.mbt").join(","), content="src/main.mbt,src/lib/util.mbt")
}

///|
test "glob expansion preserves input order" {
  let paths = ["src/b.mbt", "src/a.mbt", "src/nested/c.mbt"]
  inspect(expand_glob(paths, "src/**/*.mbt").join(","), content="src/b.mbt,src/a.mbt,src/nested/c.mbt")
}

///|
test "glob expansion accepts multiple patterns and deduplicates paths" {
  let paths = [
    "src/app.mbt",
    "src/app.test.mbt",
    "README.md",
    "dist/app.mbt",
  ]
  inspect(
    expand_globs(paths, ["src/**/*.mbt", "README.md", "src/**/*.test.mbt"]).join(","),
    content="src/app.mbt,src/app.test.mbt,README.md",
  )
}

///|
test "glob expansion supports negation patterns" {
  let paths = [
    "src/app.mbt",
    "src/app.test.mbt",
    "src/nested/lib.mbt",
    "dist/app.mbt",
  ]
  inspect(
    expand_globs(paths, ["src/**/*.mbt", "!src/**/*.test.mbt"]).join(","),
    content="src/app.mbt,src/nested/lib.mbt",
  )
}

///|
test "glob expansion reports empty matches unless allowed" {
  let paths = ["src/app.mbt"]
  let missing = expand_globs_checked(paths, ["test/**/*.mbt"], allow_empty=false)
  inspect(missing is Err(_), content="true")
  inspect(
    missing.unwrap_err().message(),
    content="Glob error: no files matched: test/**/*.mbt",
  )
  let allowed = expand_globs_checked(paths, ["test/**/*.mbt"], allow_empty=true)
  inspect(allowed.unwrap().length(), content="0")
}

///|
test "glob expansion can include dotfiles with dot option" {
  let paths = ["src/app.mbt", "src/.hidden.mbt", "src/nested/.env.mbt"]
  inspect(
    expand_globs_with_options(paths, ["src/**/*.mbt"], dot=false).join(","),
    content="src/app.mbt",
  )
  inspect(
    expand_globs_with_options(paths, ["src/**/*.mbt"], dot=true).join(","),
    content="src/app.mbt,src/.hidden.mbt,src/nested/.env.mbt",
  )
}

///|
test "glob expansion keeps input path order across includes and negations" {
  let paths = [
    "src/c.mbt",
    "test/a.mbt",
    "src/b.test.mbt",
    "src/a.mbt",
  ]
  inspect(
    expand_globs(paths, ["test/**/*.mbt", "src/**/*.mbt", "!src/**/*.test.mbt"]).join(","),
    content="src/c.mbt,test/a.mbt,src/a.mbt",
  )
}

///|
test "glob double star matches zero or many path segments" {
  inspect(glob_matches("src/**/*.mbt", "src/app.mbt"), content="true")
  inspect(glob_matches("src/**/*.mbt", "src/lib/nested/app.mbt"), content="true")
  inspect(glob_matches("src/**/*.mbt", "src/app.txt"), content="false")
}
