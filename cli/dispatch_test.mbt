///|
fn dispatch_config_source(entry : String, default_task : String) -> String {
  "[package]\nname = \"site-build\"\n\n[mulp]\nentry = \"" +
  entry +
  "\"\ndefault = \"" +
  default_task +
  "\""
}

///|
fn dispatch_task(_ctx : @core.Context) -> Result[Unit, @core.MulpError] {
  Ok(())
}

///|
fn dispatch_registry() -> @core.Registry {
  let registry = @core.new_registry()
  ignore(@core.task(registry, "build", dispatch_task))
  ignore(@core.task(registry, "test", dispatch_task))
  registry
}

///|
test "dispatch builds proxy command from discovered config and default task" {
  let files = [("/workspace/app/mulp.mbtx", "fn main { println(\"build\") }")]
  let result = dispatch_cli(
    cwd="/workspace/app/src",
    files~,
    args=[],
    registry=dispatch_registry(),
  ).unwrap()
  match result {
    Proxy(command) => {
      inspect(command.executable, content="moon")
      inspect(command.args.join(" "), content="run --quiet --target native /workspace/app/mulp.mbtx --")
    }
    _ => inspect("wrong dispatch", content="proxy")
  }
}

///|
test "dispatch proxies explicit task to discovered mulp mbtx" {
  let files = [("/workspace/app/mulp.mbtx", "fn main { println(\"build\") }")]
  let result = dispatch_cli(
    cwd="/workspace/app/src",
    files~,
    args=["build"],
    registry=dispatch_registry(),
  ).unwrap()
  match result {
    Proxy(command) =>
      inspect(
        command.args.join(" "),
        content="run --quiet --target native /workspace/app/mulp.mbtx -- build",
      )
    _ => inspect("wrong dispatch", content="proxy")
  }
}

///|
test "dispatch proxies watch task to discovered mulp mbtx" {
  let files = [("/workspace/app/mulp.mbtx", "fn main { println(\"build\") }")]
  let result = dispatch_cli(
    cwd="/workspace/app/src",
    files~,
    args=["--watch", "build"],
    registry=dispatch_registry(),
  ).unwrap()
  match result {
    Proxy(command) =>
      inspect(
        command.args.join(" "),
        content="run --quiet --target native /workspace/app/mulp.mbtx -- --watch build",
      )
    _ => inspect("wrong dispatch", content="proxy")
  }
}

///|
test "dispatch proxies task listing to mulp mbtx instead of local registry" {
  let files = [("/workspace/app/mulp.mbtx", "fn main { println(\"build\") }")]
  let result = dispatch_cli(
    cwd="/workspace/app",
    files~,
    args=["--tasks"],
    registry=dispatch_registry(),
  ).unwrap()
  match result {
    Proxy(command) =>
      inspect(
        command.args.join(" "),
        content="run --quiet --target native /workspace/app/mulp.mbtx -- --tasks",
      )
    _ => inspect("wrong dispatch", content="proxy")
  }
}

///|
test "dispatch normalizes tasks-simple for mulp mbtx" {
  let files = [("/workspace/app/mulp.mbtx", "fn main { println(\"build\") }")]
  let result = dispatch_cli(
    cwd="/workspace/app",
    files~,
    args=["--tasks-simple"],
    registry=dispatch_registry(),
  ).unwrap()
  match result {
    Proxy(command) =>
      inspect(
        command.args.join(" "),
        content="run --quiet --target native /workspace/app/mulp.mbtx -- --tasks",
      )
    _ => inspect("wrong dispatch", content="proxy")
  }
}

///|
test "dispatch normalizes short tasks alias for mulp mbtx" {
  let files = [("/workspace/app/mulp.mbtx", "fn main { println(\"build\") }")]
  let result = dispatch_cli(
    cwd="/workspace/app",
    files~,
    args=["-T"],
    registry=dispatch_registry(),
  ).unwrap()
  match result {
    Proxy(command) =>
      inspect(
        command.args.join(" "),
        content="run --quiet --target native /workspace/app/mulp.mbtx -- --tasks",
      )
    _ => inspect("wrong dispatch", content="proxy")
  }
}

///|
test "dispatch uses explicit config path" {
  let files = [
    ("/workspace/app/mulp.mbtx", "fn main { println(\"build\") }"),
    ("/tmp/custom.mbtx", "fn main { println(\"custom\") }"),
  ]
  let result = dispatch_cli(
    cwd="/workspace/app",
    files~,
    args=["--config", "/tmp/custom.mbtx"],
    registry=dispatch_registry(),
  ).unwrap()
  match result {
    Proxy(command) =>
      inspect(command.args.join(" "), content="run --quiet --target native /tmp/custom.mbtx --")
    _ => inspect("wrong dispatch", content="proxy")
  }
}

///|
test "dispatch uses explicit mulpfile alias" {
  let files = [
    ("/workspace/app/mulp.mbt", dispatch_config_source("src/build", "build")),
    ("/tmp/custom.mbt", dispatch_config_source("custom/build", "custom")),
  ]
  let result = dispatch_cli(
    cwd="/workspace/app",
    files~,
    args=["--mulpfile", "/tmp/custom.mbt"],
    registry=dispatch_registry(),
  ).unwrap()
  match result {
    Proxy(command) =>
      inspect(command.args.join(" "), content="run custom/build -- custom")
    _ => inspect("wrong dispatch", content="proxy")
  }
}

///|
test "dispatch renders task list for explicit legacy config" {
  let files = [
    ("/workspace/app/mulp.mbt", dispatch_config_source("src/build", "build")),
  ]
  let result = dispatch_cli(
    cwd="/workspace/app",
    files~,
    args=["--config", "/workspace/app/mulp.mbt", "--tasks"],
    registry=dispatch_registry(),
  ).unwrap()
  match result {
    Rendered(output) => inspect(output, content="build\ntest")
    _ => inspect("wrong dispatch", content="rendered")
  }
}

///|
test "dispatch renders tasks json for explicit legacy config" {
  let files = [
    ("/workspace/app/mulp.mbt", dispatch_config_source("src/build", "build")),
  ]
  let result = dispatch_cli(
    cwd="/workspace/app",
    files~,
    args=["--config", "/workspace/app/mulp.mbt", "--tasks-json"],
    registry=dispatch_registry(),
  ).unwrap()
  match result {
    Rendered(json) => inspect(json.has_prefix("{\"label\":\"Tasks\""), content="true")
    _ => inspect("wrong dispatch", content="rendered")
  }
}

///|
test "dispatch renders task tree for explicit legacy config" {
  let files = [
    ("/workspace/app/mulp.mbt", dispatch_config_source("src/build", "build")),
  ]
  let result = dispatch_cli(
    cwd="/workspace/app",
    files~,
    args=["--config", "/workspace/app/mulp.mbt", "--tree", "build"],
    registry=dispatch_registry(),
  ).unwrap()
  match result {
    Rendered(output) => inspect(output, content="root\n  build\n  test")
    _ => inspect("wrong dispatch", content="rendered")
  }
}
