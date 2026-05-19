///|
fn shallow_tree(registry : @core.AsyncRegistry) -> @core.TreeNode {
  let children : Array[@core.TreeNode] = []
  for name in registry.names() {
    children.push(@core.new_tree_node(name, []))
  }
  @core.new_tree_node("root", children)
}

///|
pub fn Undertaker::tree(
  self : Undertaker,
  deep? : Bool = false,
) -> @core.TreeNode {
  if deep {
    self.registry.tree()
  } else {
    shallow_tree(self.registry)
  }
}
