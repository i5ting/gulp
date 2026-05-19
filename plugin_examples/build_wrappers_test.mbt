///|
test "plugin examples build moonbit build wrapper commands" {
  let command = moonbit_build_wrapper(target="native", package_name="i5ting/gulp")
  inspect(command.program(), content="moon")
  inspect(command.args(), content="[\"build\", \"--target\", \"native\", \"i5ting/gulp\"]")
}

///|
test "plugin examples build typescript wrapper commands" {
  let command = typescript_build_wrapper(project="tsconfig.json")
  inspect(command.program(), content="npx")
  inspect(command.args(), content="[\"tsc\", \"--project\", \"tsconfig.json\"]")
}
