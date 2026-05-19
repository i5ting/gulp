///|
pub(all) enum VinylOption[A] {
  Fixed(A)
  ForFile((@vinyl.VinylFile) -> A)
}

///|
pub fn[A] fixed_option(value : A) -> VinylOption[A] {
  Fixed(value)
}

///|
pub fn[A] file_option(resolve : (@vinyl.VinylFile) -> A) -> VinylOption[A] {
  ForFile(resolve)
}

///|
pub fn[A] VinylOption::resolve(
  self : VinylOption[A],
  file : @vinyl.VinylFile,
) -> A {
  match self {
    Fixed(value) => value
    ForFile(resolve) => resolve(file)
  }
}

///|
pub fn[A] resolve_option_or(
  option : VinylOption[A]?,
  file : @vinyl.VinylFile,
  default : A,
) -> A {
  match option {
    Some(value) => value.resolve(file)
    None => default
  }
}
