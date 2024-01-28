#include <filesystem>
#include <iostream>

#include "otk/otk.hpp"

int main() {
  // auto odb_path = otk::find_file(std::filesystem::current_path());
  auto odb_path = std::filesystem::current_path() / "viewer_tutorial.odb";

  otk::Odb odb(odb_path);
  std::cout << odb;

  // odb.write_vtu();
  odb.write_info();

  return 0;
}