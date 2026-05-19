///|
test "context carries cwd and current time" {
  let token = new_cancellation_token()
  let ctx = new_context(cwd="/workspace/app", now_ms=1200L, cancellation=token)
  inspect(ctx.cwd, content="/workspace/app")
  inspect(ctx.now_ms, content="1200")
  inspect(ctx.is_cancelled(), content="false")
}

///|
test "cancellation token can be requested" {
  let token = new_cancellation_token()
  let ctx = new_context(cwd="/workspace/app", now_ms=1200L, cancellation=token)
  token.cancel()
  inspect(ctx.is_cancelled(), content="true")
}
