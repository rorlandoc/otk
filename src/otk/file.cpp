#include "otk/file.hpp"

#include <fmt/format.h>

#include <iostream>

namespace otk {

// ---------------------------------------------------------------------------------------
//
//   File finder utility to navigate the file system
//
// ---------------------------------------------------------------------------------------
fs::path find_file(fs::path current_path) {
    // Gathering files and directories in the current directory
    std::vector<fs::path> files;
    for (const auto &entry : fs::directory_iterator(current_path)) {
        if (entry.is_directory() || entry.path().extension() == ".odb")
            files.push_back(entry.path());
    }

    // Getting the width of the index column
    int width = int(std::log10(files.size())) + 1;
    std::string index_fmt = "{:" + fmt::format("{}", width) + "d}";

    // Printing first line of CLI
    fmt::print("Contents of {}\n\n", current_path.string());

    // Print a line for each file in the directory
    int i = 0;
    fmt::print(fmt::runtime(index_fmt + ": ..\n"), i++);
    for (const auto &file : files) {
        fmt::print(fmt::runtime(index_fmt + ": {}\n"), i++, file.filename().string());
    }
    fmt::print(fmt::runtime(index_fmt + ": Exit\n"), i);

    int file_index = 0;
    bool valid_input = false;
    // Loop until the user selects a valid ODB file
    while (!valid_input) {
        // Prompt user for file selection
        fmt::print("\nSelect file to open: ");
        std::cin >> file_index;

        // Check if the input is valid , i.e belongs in [0, files.size() + 1]
        bool index_valid = (file_index > -1 && file_index <= files.size() + 1);
        if (index_valid) {
            // Check if the user wants to go back to the parent directory
            bool is_parent_dir = (file_index == 0);
            bool is_exit = (file_index == files.size() + 1);
            if (is_parent_dir) {
                // Recursively call find_file with the parent directory
                return find_file(current_path.parent_path());
            } else if (is_exit) {
                // Exit the program
                exit(0);
            } else {
                // Check if the selected file is a directory
                bool is_dir = fs::is_directory(files[file_index - 1]);
                if (is_dir) {
                    // Recursively call find_file with the selected directory
                    return find_file(files[file_index - 1]);
                } else {
                    // Check if the selected file is an ODB file
                    bool is_odb_file = (files[file_index - 1].extension() == ".odb");
                    if (is_odb_file) {
                        valid_input = true;
                    } else {
                        fmt::print("File selected is not an ODB file. Try again.\n");
                    }
                }
            }
        } else {
            fmt::print("Invalid input. Try again.\n");
        }
    }
    fs::path file_path = files[file_index - 1];
    fmt::print("\n");
    return file_path.string();
}

}  // namespace otk