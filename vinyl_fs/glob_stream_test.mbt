///|
test "vinyl-fs glob stream preserves include and negation order" {
  let paths = ["a.txt", "b.txt", "nested/c.txt"]
  inspect(
    select_glob_stream_paths(paths, ["!b.txt", "*.txt"], dot=false),
    content="[\"a.txt\", \"b.txt\"]",
  )
  inspect(
    select_glob_stream_paths(paths, ["*.txt", "!b.txt"], dot=false),
    content="[\"a.txt\"]",
  )
}

///|
test "vinyl-fs glob stream applies dot option" {
  let paths = ["a.txt", ".hidden.txt", "nested/.deep.txt"]
  inspect(
    select_glob_stream_paths(paths, ["**/*.txt"], dot=false),
    content="[\"a.txt\"]",
  )
  inspect(
    select_glob_stream_paths(paths, ["**/*.txt"], dot=true),
    content="[\"a.txt\", \".hidden.txt\", \"nested/.deep.txt\"]",
  )
}
