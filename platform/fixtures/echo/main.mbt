///|
fn main {
  let args = @env.args()
  if args.length() > 1 {
    println("mulp fixture: " + args[1])
  } else {
    println("mulp fixture:")
  }
}
