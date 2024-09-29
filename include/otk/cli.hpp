#ifndef OTK_CLI_HPP
#define OTK_CLI_HPP

#include <filesystem>
#include <string>

#include <argparse/argparse.hpp>

namespace fs = std::filesystem;

namespace otk {

// ---------------------------------------------------------------------------------------
//
//   Name and description constants
//
// ---------------------------------------------------------------------------------------
const std::string NAME = "OTK";
const std::string DESCRIPTION = "ODB<->VTK Toolkit";

// ---------------------------------------------------------------------------------------
//
//   Constants for formatting
//
// ---------------------------------------------------------------------------------------
constexpr int WIDTH = 50;
const std::string SEPARATOR_1 = "==================================================";
const std::string SEPARATOR_2 = "--------------------------------------------------";

// ---------------------------------------------------------------------------------------
//
//   File finder utility to navigate the file system
//
// ---------------------------------------------------------------------------------------
fs::path find_file(fs::path current_path);

// ---------------------------------------------------------------------------------------
//
//   Functions
//
// ---------------------------------------------------------------------------------------
void print_header(const std::string& version, const std::string& build);

void print_footer();

void print_error(const std::string& error_message,
                 const argparse::ArgumentParser* options = nullptr);

void print_help(const argparse::ArgumentParser* options);

void print_title(const std::string& title);

void print_separator_1(int count = 1);

void print_separator_2(int count = 1);

void clear_screen();

std::string format_byte_size(size_t size);

}  // namespace otk

#endif  // !OTK_CLI_HPP