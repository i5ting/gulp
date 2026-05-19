///|
pub struct PackageConfig {
  name : String
  version : String?
}

///|
pub struct MulpSection {
  entry : String
  default_task : String
}

///|
pub struct MulpConfig {
  pkg : PackageConfig
  mulp : MulpSection
}

///|
fn split_assignment(line : String) -> Result[(String, String), @core.MulpError] {
  let parts : Array[String] = []
  for part in line.split("=") {
    parts.push(part.trim().to_owned())
  }
  if parts.length() != 2 {
    Err(@core.config_error("malformed line: " + line))
  } else {
    Ok((parts[0], parts[1]))
  }
}

///|
fn parse_quoted_value(
  key : String,
  value : String,
) -> Result[String, @core.MulpError] {
  if value.length() >= 2 && value.has_prefix("\"") && value.has_suffix("\"") {
    Ok(value[1:value.length() - 1].to_owned())
  } else {
    Err(@core.config_error("value for " + key + " must be quoted"))
  }
}

///|
fn section_name(line : String) -> Result[String?, @core.MulpError] {
  if line.has_prefix("[") {
    if line.has_suffix("]") {
      let name = line[1:line.length() - 1].to_owned()
      if name == "package" || name == "mulp" {
        Ok(Some(name))
      } else {
        Err(@core.config_error("unknown section: " + name))
      }
    } else {
      Err(@core.config_error("malformed section: " + line))
    }
  } else {
    Ok(None)
  }
}

///|
fn find_substring(source : String, needle : String) -> Int {
  if needle == "" {
    0
  } else if needle.length() > source.length() {
    -1
  } else {
    for index in 0..=(source.length() - needle.length()) {
      if source[index:index + needle.length()].to_owned() == needle {
        return index
      }
    }
    -1
  }
}

///|
fn quoted_return_after(source : String, marker : String) -> Result[String, @core.MulpError] {
  let start = find_substring(source, marker)
  if start < 0 {
    Err(@core.config_error("missing " + marker))
  } else {
    let mut first_quote = -1
    for index in start..<source.length() {
      if source[index:index + 1].to_owned() == "\"" {
        first_quote = index
        break
      }
    }
    if first_quote < 0 {
      Err(@core.config_error("missing quoted return for " + marker))
    } else {
      for index in (first_quote + 1)..<source.length() {
        if source[index:index + 1].to_owned() == "\"" {
          return Ok(source[first_quote + 1:index].to_owned())
        }
      }
      Err(@core.config_error("missing quoted return for " + marker))
    }
  }
}

///|
fn parse_mulp_mbt_config(source : String) -> Result[MulpConfig, @core.MulpError] {
  let package_name = match quoted_return_after(source, "pub fn package_name") {
    Ok(name) => name
    Err(_) => "mulp"
  }
  let entry = match quoted_return_after(source, "pub fn entry_package") {
    Ok(name) => name
    Err(_) => ""
  }
  let default_task = match quoted_return_after(source, "pub fn default_task") {
    Ok(name) => name
    Err(err) => return Err(err)
  }
  Ok({
    pkg: { name: package_name, version: None },
    mulp: { entry, default_task },
  })
}

///|
pub fn parse_config(source : String) -> Result[MulpConfig, @core.MulpError] {
  if find_substring(source, "pub fn default_task") >= 0 {
    return parse_mulp_mbt_config(source)
  }
  let mut section = ""
  let mut package_name = ""
  let mut package_version : String? = None
  let mut entry = ""
  let mut default_task = ""
  for view in source.split("\n") {
    let line = view.trim().to_owned()
    if line == "" || line.has_prefix("#") {
      continue
    }
    match section_name(line) {
      Ok(Some(name)) => section = name
      Ok(None) => {
        let (key, raw_value) = match split_assignment(line) {
          Ok(pair) => pair
          Err(err) => return Err(err)
        }
        if section == "" {
          return Err(@core.config_error("key outside section: " + key))
        }
        let full_key = section + "." + key
        let value = match parse_quoted_value(full_key, raw_value) {
          Ok(parsed) => parsed
          Err(err) => return Err(err)
        }
        if section == "package" {
          match key {
            "name" => package_name = value
            "version" => package_version = Some(value)
            _ => return Err(@core.config_error("unknown key: " + full_key))
          }
        } else if section == "mulp" {
          match key {
            "entry" => entry = value
            "default" => default_task = value
            _ => return Err(@core.config_error("unknown key: " + full_key))
          }
        }
      }
      Err(err) => return Err(err)
    }
  }
  if package_name == "" {
    Err(@core.config_error("missing package.name"))
  } else if default_task == "" {
    Err(@core.config_error("missing mulp.default"))
  } else {
    Ok({
      pkg: { name: package_name, version: package_version },
      mulp: { entry, default_task },
    })
  }
}
