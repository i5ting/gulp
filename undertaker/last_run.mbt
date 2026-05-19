///|
fn apply_precision(
  value : Int64,
  precision : Int64?,
) -> Result[Int64, @core.MulpError] {
  match precision {
    None => Ok(value)
    Some(step) =>
      if step <= 0L {
        Err(@core.config_error("precision must be positive"))
      } else {
        Ok(value / step * step)
      }
  }
}

///|
fn validate_precision(precision : Int64?) -> Result[Unit, @core.MulpError] {
  match precision {
    Some(step) if step <= 0L =>
      Err(@core.config_error("precision must be positive"))
    _ => Ok(())
  }
}

///|
pub fn Undertaker::last_run(
  self : Undertaker,
  name : String,
  precision~ : Int64?,
) -> Result[Int64?, @core.MulpError] {
  match validate_precision(precision) {
    Err(err) => Err(err)
    Ok(_) =>
      match self.registry.require(name) {
        Err(err) => Err(err)
        Ok(task) =>
          match @core.last_run_async(task) {
            None => Ok(None)
            Some(value) =>
              match apply_precision(value, precision) {
                Ok(next) => Ok(Some(next))
                Err(err) => Err(err)
              }
          }
      }
  }
}
