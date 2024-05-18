#include <filesystem>
#include <sstream>

#include <argparse/argparse.hpp>

#include <fmt/format.h>

#include "otk/file.hpp"
#include "otk/odb.hpp"

#pragma message("OTK build version: " STR(OTK_VERSION))

namespace fs = std::filesystem;

int main(int argc, char *argv[]) {
    fmt::print("==================================================\n");
    fmt::print("==================================================\n");
    fmt::print("{:^50}\n", fmt::format("OTK {}", STR(OTK_VERSION)));
    fmt::print("--------------------------------------------------\n");
    fmt::print("{:^50}\n", "ODB<->VTK Toolkit");
    fmt::print("--------------------------------------------------\n");
    fmt::print("{:^50}\n", fmt::format("Build {}", STR(OTK_BUILD)));
    fmt::print("--------------------------------------------------\n");

    argparse::ArgumentParser options("otk_cli", STR(OTK_VERSION));

    options.add_argument("file").help("ODB file name").default_value(std::string{});
    options.add_argument("--info", "-i")
        .help("Get info on the ODB")
        .default_value(false)
        .implicit_value(true)
        .nargs(0);
    options.add_argument("--verbose", "-v")
        .help("Get info on the analysis Steps")
        .default_value(false)
        .implicit_value(true)
        .nargs(0);

    try {
        options.parse_args(argc, argv);
    } catch (const std::exception &err) {
        fmt::print("--------------------------------------------------\n");
        fmt::print("ERROR: {}\n\n", err.what());
        std::stringstream ss;
        ss << options;
        fmt::print("{}", ss.str());
        fmt::print("==================================================\n");
        fmt::print("==================================================\n");
        return 1;
    }

    fs::path file;
    if (options.get<std::string>("file").empty()) {
        file = otk::find_file(fs::current_path());
    } else {
        file = fs::path{options.get<std::string>("file")};
    }

    try {
        otk::Odb odb{file};

        if (options["--info"] == true) {
            fmt::print("--------------------------------------------------\n");
            odb.odb_info(options["--verbose"] == true);
            fmt::print("==================================================\n");
            fmt::print("==================================================\n");
            return 0;
        }

        fmt::print("==================================================\n");
        fmt::print("==================================================\n");
        return 0;
    } catch (const std::exception &err) {
        fmt::print("--------------------------------------------------\n");
        fmt::print("ERROR: {}\n\n", err.what());
        fmt::print("==================================================\n");
        fmt::print("==================================================\n");
        return 1;
    }
}
