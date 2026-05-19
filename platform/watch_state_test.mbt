///|
test "watch run state marks pending rebuild during active run" {
  let state = watch_run_state()
  state.start_run()
  state.mark_change()
  inspect(state.has_pending_rebuild(), content="true")
  inspect(state.finish_run(), content="true")
  inspect(state.has_pending_rebuild(), content="false")
}

///|
test "watch run state stays active after task failure" {
  let state = watch_run_state()
  state.start_run()
  ignore(state.finish_failed_run("boom"))
  inspect(state.is_active(), content="true")
  inspect(state.is_running(), content="false")
  inspect(state.last_failure().unwrap(), content="boom")
}

///|
test "watch run state stops active run and cancels token" {
  let state = watch_run_state()
  let token = @core.new_cancellation_token()
  state.start_run()
  state.mark_change()
  state.stop(token)
  inspect(state.is_active(), content="false")
  inspect(state.is_running(), content="false")
  inspect(state.has_pending_rebuild(), content="false")
  inspect(token.is_cancelled(), content="true")
}
