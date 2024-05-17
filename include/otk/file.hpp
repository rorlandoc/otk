#ifndef OTK_FILE_HPP
#define OTK_FILE_HPP

#include <filesystem>

namespace fs = std::filesystem;

namespace otk {

// ---------------------------------------------------------------------------------------
//
//   File finder utility to navigate the file system
//
// ---------------------------------------------------------------------------------------
fs::path find_file(fs::path current_path);

}  // namespace otk

#endif  // !OTK_FILE_HPP