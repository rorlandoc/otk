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
    options.add_argument("--info", "-I")
        .help("Get info on the ODB")
        .default_value(false)
        .implicit_value(true)
        .nargs(0);
    options.add_argument("--steps", "-s")
        .help("Get info on the analysis Steps")
        .default_value(false)
        .implicit_value(true)
        .nargs(0);
    options.add_argument("--frames", "-fr")
        .help("Get info on the Frames of a given step")
        .default_value(std::vector<std::string>{})
        .append();
    options.add_argument("--fields", "-fi")
        .help("Get info on the Fields of a given frame of a given step")
        .default_value(std::vector<std::string>{})
        .nargs(2)
        .append();
    options.add_argument("--instances", "-i")
        .help("Get info on the Instances")
        .default_value(false)
        .implicit_value(true)
        .nargs(0);
    options.add_argument("--elements", "-el")
        .help("Get info on the Elements of a given instance")
        .default_value(std::vector<std::string>{})
        .append();
    options.add_argument("--nodes", "-nd")
        .help("Get info on the Nodes of a given instance")
        .default_value(std::vector<std::string>{})
        .append();

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
            odb.info();
        }

        if (options["--steps"] == true) {
            fmt::print("--------------------------------------------------\n");
            odb.steps();
        }

        auto step_names = options.get<std::vector<std::string>>("--frames");
        if (step_names.size() > 0) {
            for (const auto &step_name : step_names) {
                fmt::print("--------------------------------------------------\n");
                odb.frames(step_name);
            }
        }

        auto step_frame_pairs = options.get<std::vector<std::string>>("--fields");
        if (step_frame_pairs.size() > 0 && step_frame_pairs.size() % 2 == 0) {
            for (size_t i = 0; i < step_frame_pairs.size(); i += 2) {
                fmt::print("--------------------------------------------------\n");
                odb.fields(step_frame_pairs[i], std::stoi(step_frame_pairs[i + 1]));
            }
        }

        if (options["--instances"] == true) {
            fmt::print("--------------------------------------------------\n");
            odb.instances();
        }

        auto instance_names = options.get<std::vector<std::string>>("--elements");
        if (instance_names.size() > 0) {
            for (const auto &instance_name : instance_names) {
                fmt::print("--------------------------------------------------\n");
                odb.elements(instance_name);
            }
        }

        instance_names = options.get<std::vector<std::string>>("--nodes");
        if (instance_names.size() > 0) {
            for (const auto &instance_name : instance_names) {
                fmt::print("--------------------------------------------------\n");
                odb.nodes(instance_name);
            }
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
