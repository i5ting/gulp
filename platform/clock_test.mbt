///|
test "manual clock can advance deterministically" {
  let clock = manual_clock(1000L)
  inspect(clock.now_ms(), content="1000")
  clock.advance_ms(250L)
  inspect(clock.now_ms(), content="1250")
}

///|
test "system clock returns epoch milliseconds" {
  inspect(system_clock().now_ms() > 0L, content="true")
}
