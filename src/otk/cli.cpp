#include "otk/cli.hpp"

#include <sstream>

#include <fmt/format.h>

namespace otk {

// ---------------------------------------------------------------------------------------
//
//   Print the main header
//
// ---------------------------------------------------------------------------------------
void print_header(const std::string& version, const std::string& build) {
    clear_screen();
    print_separator_1(2);
    print_title(fmt::format("{} {}", NAME, version));
    print_separator_2();
    print_title(DESCRIPTION);
    print_separator_2();
    print_title(fmt::format("Build {}", build));
    print_separator_2();
}

// ---------------------------------------------------------------------------------------
//
//   Print the main footer
//
// ---------------------------------------------------------------------------------------
void print_footer() { print_separator_1(2); }

// ---------------------------------------------------------------------------------------
//
//   Print an error message
//
// ---------------------------------------------------------------------------------------
void print_error(const std::string& error_message,
                 const argparse::ArgumentParser* options) {
    fmt::print("\n");
    print_separator_2();
    fmt::print("ERROR: {}\n\n", error_message);
    print_help(options);
    print_footer();
}

// ---------------------------------------------------------------------------------------
//
//   Print the help message
//
// ---------------------------------------------------------------------------------------`
void print_help(const argparse::ArgumentParser* options) {
    if (options) {
        std::stringstream ss;
        ss << options;
        fmt::print("{}", ss.str());
    }
}

// ---------------------------------------------------------------------------------------
//
//   Print a title
//
// ---------------------------------------------------------------------------------------
void print_title(const std::string& title) { fmt::print("{:^{}}\n", title, WIDTH); }

// ---------------------------------------------------------------------------------------
//
//   Print a separator (level 1)
//
// ---------------------------------------------------------------------------------------
void print_separator_1(int count) {
    for (int i = 0; i < count; ++i) {
        fmt::print("{}\n", SEPARATOR_1);
    }
}

// ---------------------------------------------------------------------------------------
//
//   Print a separator (level 2)
//
// ---------------------------------------------------------------------------------------
void print_separator_2(int count) {
    for (int i = 0; i < count; ++i) {
        fmt::print("{}\n", SEPARATOR_2);
    }
}

// ---------------------------------------------------------------------------------------
//
//   Clear the screen
//
// ---------------------------------------------------------------------------------------
void clear_screen() {
#if defined(_WIN32) || defined(_WIN64)
    system("cls");
#elif defined(__linux__) || defined(__unix__)
    fmt::print(u8"\033[2J\033[1;1H");
#endif
}

// ---------------------------------------------------------------------------------------
//
//   Format the size of a file in bytes
//
// ---------------------------------------------------------------------------------------
std::string format_byte_size(size_t size) {
    constexpr unsigned long long KB{1024ull};
    constexpr unsigned long long MB{1024ull * KB};
    constexpr unsigned long long GB{1024ull * MB};
    constexpr unsigned long long TB{1024ull * GB};

    constexpr long double KB_F{1024.0};
    constexpr long double MB_F{1024.0 * KB_F};
    constexpr long double GB_F{1024.0 * MB_F};
    constexpr long double TB_F{1024.0 * GB_F};

    if (size < KB) {
        return fmt::format("{} B", size);
    }
    if (size < MB) {
        return fmt::format("{:.2f} KB", size / KB_F);
    }
    if (size < GB) {
        return fmt::format("{:.2f} MB", size / MB_F);
    }
    if (size < TB) {
        return fmt::format("{:.2f} GB", size / GB_F);
    }
    return fmt::format("{:.2f} TB", size / TB_F);
}

// ---------------------------------------------------------------------------------------
//
//   File finder utility to navigate the file system
//
// ---------------------------------------------------------------------------------------
fs::path find_file(fs::path current_path) {
    // Gathering files and directories in the current directory
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(current_path)) {
        if (entry.is_directory() || entry.path().extension() == ".odb")
            files.push_back(entry.path());
    }

    // Getting the width of the index column
    int width = int(std::log10(files.size())) + 1;
    std::string index_fmt = "{:" + fmt::format("{}", width) + "d}";

    // Clearing the screen
    clear_screen();

    // Printing first line of CLI
    fmt::print("Contents of {}\n\n", current_path.string());

    // Print a line for each file in the directory
    int i = 0;
    fmt::print(fmt::runtime(index_fmt + ": ..\n"), i++);
    for (const auto& file : files) {
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