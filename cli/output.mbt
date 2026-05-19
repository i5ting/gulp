///|
pub fn render_cli_output(
  cli : CliArgs,
  registry : @core.Registry,
) -> Result[String, @core.MulpError] {
  match cli.action {
    ListTasks   => Ok(registry.names().join("\n"))
    TasksJson   => Ok(registry.tasks_json())
    Tree(_)     => Ok(registry.tree().render())
    Run(_) | Watch(_) => Err(@core.config_error("run is not a renderable CLI output"))
  }
}
