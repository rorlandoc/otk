#include <filesystem>
#include <iostream>

#include <argparse/argparse.hpp>

#include "otk/otk.hpp"

#pragma message("OTK build version: " STR(OTK_VERSION))

int main(int argc, char *argv[])
{
    argparse::ArgumentParser options("otk_cli", STR(OTK_VERSION));

    options.add_argument("file").help("ODB file name");
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
        .default_value(std::vector<std::pair<std::string, int>>{})
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

    try
    {
        options.parse_args(argc, argv);
    }
    catch (const std::exception &err)
    {
        std::cout << err.what() << "\n\n";
        std::cout << options;
        return 1;
    }

    std::filesystem::path file =
        std::filesystem::path{options.get<std::string>("file")};

    otk::Odb odb(file);

    nlohmann::json output;
    try
    {
        output["file"] = odb.name();
        output["path"] = odb.path();
        if (options["--info"] == true)
        {
            output["info"] = odb.info();
        }
        if (options["--steps"] == true)
        {
            output["steps"] = odb.steps();
        }
        if (options["--instances"] == true)
        {
            output["instances"] = odb.instances();
        }
        auto steps = options.get<std::vector<std::string>>("--frames");
        for (const auto &step : steps)
        {
            output["frames"][step] = odb.frames(step);
        }
        auto steps_frames =
            options.get<std::vector<std::pair<std::string, int>>>("--fields");
        for (const auto &[step, frame] : steps_frames)
        {
            output["fields"][step][frame] = odb.fields(step, frame);
        }
    }
    catch (const std::exception &err)
    {
        std::cout << err.what() << "\n\n";
        std::cout << output.dump(2) << "\n";
        return 1;
    }

    std::cout << output.dump(2) << "\n";
    return 0;
}
