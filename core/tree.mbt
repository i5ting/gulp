///|
pub struct TreeNode {
  name : String
  children : Array[TreeNode]
}

///|
pub fn new_tree_node(name : String, children : Array[TreeNode]) -> TreeNode {
  { name, children }
}

///|
pub fn TreeNode::name(self : TreeNode) -> String {
  self.name
}

///|
pub fn TreeNode::children(self : TreeNode) -> Array[TreeNode] {
  self.children
}

///|
fn task_tree(task : Task) -> TreeNode {
  let children : Array[TreeNode] = []
  for child in task.children() {
    children.push(task_tree(child))
  }
  { name: task.name(), children }
}

///|
pub fn Registry::tree(self : Registry) -> TreeNode {
  let children : Array[TreeNode] = []
  for task in self.tasks {
    children.push(task_tree(task))
  }
  { name: "root", children }
}

///|
fn render_tree(node : TreeNode, indent : String, lines : Array[String]) -> Unit {
  lines.push(indent + node.name)
  for child in node.children {
    render_tree(child, indent + "  ", lines)
  }
}

///|
pub fn TreeNode::render(self : TreeNode) -> String {
  let lines : Array[String] = []
  render_tree(self, "", lines)
  lines.join("\n")
}

///|
fn json_escape(s : String) -> String {
  let parts : Array[String] = []
  for c in s {
    if c == '"' {
      parts.push("\\\"")
    } else if c == '\\' {
      parts.push("\\\\")
    } else {
      parts.push(c.to_string())
    }
  }
  parts.join("")
}

///|
fn tree_node_to_json(node : TreeNode, node_type : String) -> String {
  let children_parts : Array[String] = []
  for child in node.children {
    children_parts.push(tree_node_to_json(child, "dep"))
  }
  let nodes_json = "[" + children_parts.join(",") + "]"
  "{\"label\":\"" +
  json_escape(node.name) +
  "\",\"type\":\"" +
  node_type +
  "\",\"nodes\":" +
  nodes_json +
  "}"
}

///|
fn tasks_json_from_tree(root : TreeNode) -> String {
  let parts : Array[String] = []
  for child in root.children {
    parts.push(tree_node_to_json(child, "task"))
  }
  "{\"label\":\"Tasks\",\"nodes\":[" + parts.join(",") + "]}"
}

///|
pub fn Registry::tasks_json(self : Registry) -> String {
  tasks_json_from_tree(self.tree())
}
