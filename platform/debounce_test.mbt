///|
test "debouncer fires only after the quiet window" {
  let debounce = debouncer(100L)
  debounce.push(0L)
  debounce.push(80L)
  inspect(debounce.take_ready(100L), content="false")
  inspect(debounce.take_ready(180L), content="true")
  inspect(debounce.take_ready(181L), content="false")
}
