///|
pub fn async_task_from_stream(
  name : String,
  stream : @stream.FileStream,
  signals? : Array[@core.AsyncCompletionSignal] = [],
) -> @core.AsyncTask {
  @core.new_async_task(name, async fn(
    ctx : @core.Context,
  ) -> Result[Unit, @core.MulpError] {
    let completion_signals = [@core.StreamCompletion]
    for signal in signals {
      completion_signals.push(signal)
    }
    match @core.validate_async_completion_signals(name, completion_signals) {
      Ok(_) => ()
      Err(err) => return Err(err)
    }
    @async.sleep(0)
    match stream.collect(ctx) {
      Ok(_) => Ok(())
      Err(err) => Err(err)
    }
  })
}
