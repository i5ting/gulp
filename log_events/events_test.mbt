///|
test "log events render task lifecycle messages" {
  inspect(task_start("build").render(), content="[mulp] Starting 'build'...")
  inspect(task_stop("build", duration_ms=12L).render(), content="[mulp] Finished 'build' after 12 ms")
  inspect(task_error("build", "boom").render(), content="[mulp] 'build' errored: boom")
}

///|
test "log events render ordered event streams" {
  inspect(
    render_log_events([
      task_start("clean"),
      task_stop("clean", duration_ms=1L),
    ]),
    content="[mulp] Starting 'clean'...\n[mulp] Finished 'clean' after 1 ms",
  )
}

///|
test "log events render gulp cli timestamp style messages" {
  inspect(
    task_start("build").render_with_time("20:58:55"),
    content="[20:58:55] Starting 'build'...",
  )
  inspect(
    task_stop("build", duration_ms=12L).render_with_time("20:58:56"),
    content="[20:58:56] Finished 'build' after 12 ms",
  )
  inspect(
    task_error("build", "boom").render_with_time("20:58:57"),
    content="[20:58:57] 'build' errored: boom",
  )
}

///|
test "log event recorder preserves sparkles style event order" {
  let recorder = new_log_event_recorder()
  recorder.emit(task_start("clean"))
  recorder.emit(task_stop("clean", duration_ms=1L))
  inspect(recorder.events().length(), content="2")
  inspect(
    recorder.render(),
    content="[mulp] Starting 'clean'...\n[mulp] Finished 'clean' after 1 ms",
  )
}
