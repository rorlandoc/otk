#include <filesystem>
#include <fstream>

#include <argparse/argparse.hpp>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include "otk/cli.hpp"
#include "otk/odb.hpp"

#pragma message("OTK build version: " STR(OTK_VERSION))

namespace fs = std::filesystem;

int main(int argc, char *argv[]) {
    // -----------------------------------------------------------------------------------
    //
    //   Parse the command line arguments
    //
    // -----------------------------------------------------------------------------------
    argparse::ArgumentParser options(otk::NAME, STR(OTK_VERSION));

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
        otk::print_header(STR(OTK_VERSION), STR(OTK_BUILD));
        otk::print_error(err.what(), &options);
        return 1;
    }

    // -----------------------------------------------------------------------------------
    //
    //   Get the file name
    //
    // -----------------------------------------------------------------------------------
    fs::path file;
    if (options.get<std::string>("file").empty()) {
        file = otk::find_file(fs::current_path());
    } else {
        file = fs::path{options.get<std::string>("file")};
    }

    // -----------------------------------------------------------------------------------
    //
    //   Main program
    //
    // -----------------------------------------------------------------------------------
    try {
        otk::print_header(STR(OTK_VERSION), STR(OTK_BUILD));

        // Open the ODB file
        otk::Odb odb{file};

        // Get info on the ODB file if requested
        if (options["--info"] == true) {
            otk::print_separator_2();
            odb.odb_info(options["--verbose"] == true);
            otk::print_footer();
            return 0;
        }

        // Get the json output request
        fs::path odb_path = odb.path();
        fs::path json_file = (odb_path / odb.name()).replace_extension(".json");
        if (!fs::exists(json_file)) {
            otk::print_error(fmt::format("JSON output request file {} does not exist",
                                         json_file.string()));
            return 1;
        }

        // Read the JSON file
        using namespace nlohmann;
        std::ifstream json_stream(json_file);
        json json_output_request = json::parse(json_stream);

        // Convert the ODB file to VTK

        otk::print_footer();
        return 0;
    } catch (const std::exception &err) {
        otk::print_error(err.what());
        return 1;
    }
}
