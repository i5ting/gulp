///|
pub struct BuildWrapperCommand {
  program : String
  args : Array[String]
}

///|
pub fn BuildWrapperCommand::program(self : BuildWrapperCommand) -> String {
  self.program
}

///|
pub fn BuildWrapperCommand::args(self : BuildWrapperCommand) -> Array[String] {
  let copied : Array[String] = []
  for arg in self.args {
    copied.push(arg)
  }
  copied
}

///|
pub fn moonbit_build_wrapper(
  target~ : String,
  package_name~ : String,
) -> BuildWrapperCommand {
  {
    program: "moon",
    args: ["build", "--target", target, package_name],
  }
}

///|
pub fn typescript_build_wrapper(project~ : String) -> BuildWrapperCommand {
  {
    program: "npx",
    args: ["tsc", "--project", project],
  }
}
