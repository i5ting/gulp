///|
fn main {
  let args = @env.args()
  if args.length() > 1 && args[1] == "--tasks" {
    println("app\nbuild")
  } else if args.length() > 1 && args[1] == "--tree" {
    println("root\n  app\n  build")
  } else if args.length() > 1 {
    println("mulp fixture: " + args[1])
  } else {
    println("mulp fixture:")
  }
}
