#include "otk/odb.hpp"

#include <algorithm>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <vector>

#include <fmt/format.h>

#include <odb_API.h>
#include <odb_MaterialTypes.h>
#include <odb_SectionTypes.h>

using namespace nlohmann;

namespace otk {

// ---------------------------------------------------------------------------------------
//
//   Constructor
//
// ---------------------------------------------------------------------------------------
Odb::Odb(fs::path path) {
    if (!fs::exists(path)) {
        throw std::runtime_error("File does not exist.");
    }
    if (path.extension().string() != ".odb") {
        throw std::runtime_error("File is not an ODB file.");
    }

    odb_initializeAPI();

    odb_ = &openOdb(path.string().c_str());
    path_ = path;
}

// ---------------------------------------------------------------------------------------
//
//   Destructor
//
// ---------------------------------------------------------------------------------------
Odb::~Odb() {
    odb_->close();

    odb_finalizeAPI();
}

// ---------------------------------------------------------------------------------------
//
//   General info print function (simple)
//
// ---------------------------------------------------------------------------------------
void Odb::odb_info(bool verbose) const {
    fmt::print("{:^50}\n\n", "ODB file info");
    fmt::print("Path: {}\n", path_.string());
    fmt::print("Size: {}\n", format_byte_size(this->size()));

    if (verbose) {
        fmt::print("Analysis title: {}", odb_->analysisTitle().CStr());
        fmt::print("Description: {}", odb_->description().CStr());
    }

    this->instances_info(verbose);
    this->steps_info(verbose);
}

// ---------------------------------------------------------------------------------------
//
//   Instances info print function
//
// ---------------------------------------------------------------------------------------
void Odb::instances_info(bool verbose) const {
    odb_Assembly &root_assembly = odb_->rootAssembly();
    odb_InstanceRepositoryIT instance_iterator(root_assembly.instances());

    fmt::print("Number of instances: {}\n", root_assembly.instances().size());

    for (instance_iterator.first(); !instance_iterator.isDone();
         instance_iterator.next()) {
        const odb_String &instance_name = instance_iterator.currentKey();

        fmt::print(".. {}\n", instance_name.CStr());
        this->elements_info(instance_name.CStr(), verbose);
        this->nodes_info(instance_name.CStr(), verbose);
        this->sections_info(instance_name.CStr(), verbose);
    }
}

// ---------------------------------------------------------------------------------------
//
//   Nodes info print function
//
// ---------------------------------------------------------------------------------------
void Odb::nodes_info(const std::string &instance, bool verbose) const {
    odb_Assembly &root_assembly = odb_->rootAssembly();
    const odb_Instance &instance_object =
        root_assembly.instances().constGet(instance.c_str());
    odb_Enum::odb_DimensionEnum instance_type = instance_object.embeddedSpace();
    const odb_SequenceNode &instance_nodes = instance_object.nodes();

    int number_nodes = instance_nodes.size();

    fmt::print(".... Number of nodes: {}\n", number_nodes);

    if (verbose) {
        fmt::print("       | {:^11} | {:^11} | {:^11} | {:^11} |\n", "Label", "X", "Y",
                   "Z");

        for (int i = 0; i < number_nodes; ++i) {
            const odb_Node &node = instance_nodes[i];
            if (instance_type == odb_Enum::THREE_D) {
                fmt::print("       | {:^11d} | {: 10.4e} | {: 10.4e} | {: 10.4e} |\n",
                           node.label(), node.coordinates()[0], node.coordinates()[1],
                           node.coordinates()[2]);
            } else if ((instance_type == odb_Enum::TWO_D_PLANAR) ||
                       (instance_type == odb_Enum::AXISYMMETRIC)) {
                fmt::print("       | {:^11d} | {: 10.4e} | {: 10.4e} | {:^11} |\n",
                           node.label(), node.coordinates()[0], node.coordinates()[1],
                           "           ");
            }
        }
    }
}

// ---------------------------------------------------------------------------------------
//
//   Elements info print function
//
// ---------------------------------------------------------------------------------------
void Odb::elements_info(const std::string &instance, bool verbose) const {
    odb_Assembly &root_assembly = odb_->rootAssembly();
    const odb_Instance &instance_object =
        root_assembly.instances().constGet(instance.c_str());
    const odb_SequenceElement &instance_elements = instance_object.elements();

    int number_elements = instance_elements.size();

    auto get_section_category_name = [](const odb_SectionCategory &section_category) {
        std::string section_category_raw_name{section_category.name().CStr()};
        std::string section_category_name;

        if (section_category_raw_name.find("shell < composite >") != std::string::npos) {
            section_category_name = "Shell composite";
        } else if (section_category_raw_name.find("shell") != std::string::npos) {
            section_category_name = "Shell";
        } else if (section_category_raw_name.find("solid < composite >") !=
                   std::string::npos) {
            section_category_name = "Solid composite";
        } else if (section_category_raw_name.find("solid") != std::string::npos) {
            section_category_name = "Solid";
        } else {
            section_category_name = "Other";
        }

        return section_category_name;
    };

    std::map<std::string, int> element_types;
    std::map<std::string, int> section_categories;
    for (int i = 0; i < number_elements; ++i) {
        const odb_Element &element = instance_elements[i];
        const odb_SectionCategory &section_category = element.sectionCategory();

        std::string element_type = element.type().CStr();
        std::string section_category_raw_name{section_category.name().CStr()};
        std::string section_category_name = get_section_category_name(section_category);

        if (auto it = element_types.find(element_type); it != element_types.end()) {
            it->second++;
        } else {
            element_types[element_type] = 1;
        }

        if (auto it = section_categories.find(section_category_name);
            it != section_categories.end()) {
            it->second++;
        } else {
            section_categories[section_category_name] = 1;
        }
    }

    fmt::print(".... Number of elements: {}\n", number_elements);
    for (const auto &[element_type, count] : element_types) {
        fmt::print("...... {} elements: {} \n", element_type, count);
    }
    for (const auto &[section_category, count] : section_categories) {
        fmt::print("...... {} sections: {} \n", section_category, count);
    }

    if (verbose) {
        fmt::print("       | {:^11} | {:^11} | {:^19} | {}\n", "Label", "Type", "Section",
                   "Connectivity");

        for (int i = 0; i < number_elements; ++i) {
            const odb_Element &element = instance_elements[i];
            int num_nodes = 0;
            const int *const element_connectivity = element.connectivity(num_nodes);
            const odb_SectionCategory &section_category = element.sectionCategory();
            std::string section_category_name =
                get_section_category_name(section_category);

            fmt::print("       | {:^11d} | {:^11} | {:^19} | ", element.label(),
                       element.type().CStr(), section_category_name);

            for (int j = 0; j < num_nodes; ++j) {
                fmt::print("{} ", element_connectivity[j]);
            }
            fmt::print("\n");
        }
    }
}

// ---------------------------------------------------------------------------------------
//
//   Sections info print function
//
// ---------------------------------------------------------------------------------------
void Odb::sections_info(const std::string &instance, bool verbose) const {
    odb_Assembly &root_assembly = odb_->rootAssembly();
    const odb_Instance &instance_object =
        root_assembly.instances().constGet(instance.c_str());
    const odb_SequenceSectionAssignment &section_assignments =
        instance_object.sectionAssignments();
    int num_assignments = section_assignments.size();
    fmt::print(".... Number of section assignments: {}\n", num_assignments);

    if (verbose) {
        fmt::print("       | {:^11} | {:^11} | {}\n", "Type", "Subtype", "Name");

        for (int iassignment = 0; iassignment < num_assignments; ++iassignment) {
            const odb_SectionAssignment &assignment = section_assignments[iassignment];
            const odb_Section &section = assignment.section();

            std::string section_name = section.name().CStr();
            unsigned int section_type = section.typeIdentifier();
            unsigned int section_subtype = section.subTypeIdentifier();

            fmt::print("       | {:^11d} | {:^11d} | {}\n", section_type, section_subtype,
                       section_name);
        }
    }
}

// ---------------------------------------------------------------------------------------
//
//   Steps info print function
//
// ---------------------------------------------------------------------------------------
void Odb::steps_info(bool verbose) const {
    odb_StepRepositoryIT step_iterator(odb_->steps());
    int num_steps = odb_->steps().size();

    fmt::print("Number of steps: {}\n", num_steps);

    for (step_iterator.first(); !step_iterator.isDone(); step_iterator.next()) {
        const odb_Step &step = step_iterator.currentValue();

        fmt::print(".. {} [{} frames]\n", step.name().CStr(), step.frames().size());

        this->frames_info(step.name().CStr(), verbose);
    }
}

// ---------------------------------------------------------------------------------------
//
//   Frames info print function
//
// ---------------------------------------------------------------------------------------
void Odb::frames_info(const std::string &step, bool verbose) const {
    const odb_Step &step_object = odb_->steps().constGet(step.c_str());
    const odb_SequenceFrame &frames = step_object.frames();
    int number_frames = frames.size();

    fmt::print(".... Starting value: {}\n", frames[0].frameValue());
    fmt::print(".... Ending value: {}\n", frames[number_frames - 1].frameValue());

    if (verbose) {
        fmt::print("     | {:^11} | {:^11} | {:^11} |\n", "Frame ID", "Increment",
                   "Value");

        for (int i = 0; i < number_frames; ++i) {
            const odb_Frame &frame = frames[i];
            fmt::print("     | {:^11d} | {:^11d} | {: 10.4e} |\n", frame.frameId(),
                       frame.incrementNumber(), frame.frameValue());
        }
    }

    this->fields_info(step, 0, verbose);
}

// ---------------------------------------------------------------------------------------
//
//   Fields info print function
//
// ---------------------------------------------------------------------------------------
void Odb::fields_info(const std::string &step, int frame, bool verbose) const {
    const odb_Step &step_object = odb_->steps().constGet(step.c_str());
    const odb_Frame &frame_object = step_object.frames().constGet(frame);
    const odb_FieldOutputRepository &field_outputs = frame_object.fieldOutputs();
    odb_FieldOutputRepositoryIT field_output_iterator(field_outputs);

    fmt::print(".... Number of field outputs: {}\n", field_outputs.size());

    if (verbose) {
        fmt::print("     | {:^35} | {:^7} | {:^11} | {:^7} | {:^35} |\n", "Name",
                   "Blocks", "Orientation", "Points", "Location");
    }

    for (field_output_iterator.first(); !field_output_iterator.isDone();
         field_output_iterator.next()) {
        const odb_FieldOutput &field_output = field_output_iterator.currentValue();

        if (verbose) {
            const odb_SequenceFieldLocation &locations = field_output.locations();
            int num_locations = locations.size();

            auto get_position_name = [](odb_Enum::odb_ResultPositionEnum position) {
                switch (position) {
                    case odb_Enum::odb_ResultPositionEnum::UNDEFINED_POSITION:
                        return "Undefined";
                    case odb_Enum::odb_ResultPositionEnum::NODAL:
                        return "Nodal";
                    case odb_Enum::odb_ResultPositionEnum::ELEMENT_NODAL:
                        return "Element Nodal";
                    case odb_Enum::odb_ResultPositionEnum::INTEGRATION_POINT:
                        return "Integration Point";
                    case odb_Enum::odb_ResultPositionEnum::CENTROID:
                        return "Centroid";
                    case odb_Enum::odb_ResultPositionEnum::ELEMENT_FACE:
                        return "Element Face";
                    case odb_Enum::odb_ResultPositionEnum::ELEMENT_FACE_INTEGRATION_POINT:
                        return "Element Face Integration Point";
                    case odb_Enum::odb_ResultPositionEnum::SURFACE_INTEGRATION_POINT:
                        return "Surface Integration Point";
                    case odb_Enum::odb_ResultPositionEnum::WHOLE_ELEMENT:
                        return "Whole Element";
                    case odb_Enum::odb_ResultPositionEnum::WHOLE_REGION:
                        return "Whole Region";
                    case odb_Enum::odb_ResultPositionEnum::WHOLE_PART_INSTANCE:
                        return "Whole Part Instance";
                    case odb_Enum::odb_ResultPositionEnum::WHOLE_MODEL:
                        return "Whole Model";
                    case odb_Enum::odb_ResultPositionEnum::GENERAL_PARTICLE:
                        return "General Particle";
                    case odb_Enum::odb_ResultPositionEnum::SURFACE_FACET:
                        return "Surface Facet";
                    case odb_Enum::odb_ResultPositionEnum::SURFACE_NODAL:
                        return "Surface Nodal";
                    default:
                        return "Unknown";
                }
            };

            for (int i = 0; i < num_locations; ++i) {
                const odb_FieldLocation &location = locations[i];

                fmt::print("     | {:^35} | {:^7d} | {:^11} | {:^7d} | {:^35} |\n",
                           field_output.name().CStr(),
                           field_output.bulkDataBlocks().size(),
                           field_output.hasOrientation(), location.sectionPoint().size(),
                           get_position_name(location.position()));
            }
        } else {
            fmt::print("...... {}\n", field_output.name().CStr());
        }
    }
}

// ---------------------------------------------------------------------------------------
//
//   JSON summary
//
// ---------------------------------------------------------------------------------------
json Odb::field_summary(const json &frames) const {
    json summary;

    for (auto &frame_data : frames) {
        auto step_name = frame_data["step"].get<std::string>();
        auto frame_ids = frame_data["list"].get<std::vector<int>>();

        std::cout << fmt::format("Gathering field info for {}... ", step_name);
        std::cout << std::flush;

        const odb_Step &step = odb_->steps().constGet(step_name.c_str());
        const odb_SequenceFrame &frames = step.frames();

        json step_json;
        step_json["name"] = std::string{step.name().CStr()};

        for (auto &frame_id : frame_ids) {
            const odb_Frame &frame = frames.constGet(frame_id);
            json frame_json;

            frame_json["id"] = frame.frameId();
            frame_json["increment"] = frame.incrementNumber();
            frame_json["value"] = frame.frameValue();

            const odb_FieldOutputRepository &field_outputs = frame.fieldOutputs();
            const odb_SequenceString &field_names = field_outputs.getFieldOutputNames();
            int num_fields = field_names.size();

            for (int ifield = 0; ifield < num_fields; ++ifield) {
                json field_json;
                field_json["name"] = std::string{field_names[ifield].CStr()};
                frame_json["fields"].push_back(field_json);
            }
            step_json["frames"].push_back(frame_json);
        }
        summary["steps"].push_back(step_json);

        std::cout << "done.\n" << std::flush;
    }
    return summary;
}

// ---------------------------------------------------------------------------------------
//
//   Instance summary
//
// ---------------------------------------------------------------------------------------
json Odb::instance_summary() const {
    json summary;

    odb_Assembly &root_assembly = odb_->rootAssembly();
    odb_InstanceRepositoryIT instance_iterator(root_assembly.instances());

    std::cout << "Gathering info about the instances... " << std::flush;

    for (instance_iterator.first(); !instance_iterator.isDone();
         instance_iterator.next()) {
        const std::string instance_name{instance_iterator.currentKey().CStr()};
        const odb_Instance &instance_object = instance_iterator.currentValue();
        const odb_SequenceElement &instance_elements = instance_object.elements();
        int number_elements = instance_elements.size();

        auto get_section_category_name = [](const odb_SectionCategory &section_category) {
            std::string section_category_raw_name{section_category.name().CStr()};
            std::string section_category_name;

            if (section_category_raw_name.find("shell < composite >") !=
                std::string::npos) {
                section_category_name = "Shell composite";
            } else if (section_category_raw_name.find("shell") != std::string::npos) {
                section_category_name = "Shell";
            } else if (section_category_raw_name.find("solid < composite >") !=
                       std::string::npos) {
                section_category_name = "Solid composite";
            } else if (section_category_raw_name.find("solid") != std::string::npos) {
                section_category_name = "Solid";
            } else {
                section_category_name = "Other";
            }

            return section_category_name;
        };

        std::set<std::string> element_types;
        std::set<std::string> section_categories;
        for (int i = 0; i < number_elements; ++i) {
            const odb_Element &element = instance_elements[i];
            const odb_SectionCategory &section_category = element.sectionCategory();

            std::string element_type = element.type().CStr();
            std::string section_category_raw_name{section_category.name().CStr()};
            std::string section_category_name =
                get_section_category_name(section_category);

            element_types.insert(element_type);
            section_categories.insert(section_category_name);
        }

        bool supported = true;
        bool has_composite_section = false;
        bool has_non_composite_section = false;
        for (const auto &section_category : section_categories) {
            if (section_category.find("composite") != std::string::npos) {
                has_composite_section = true;
            } else {
                has_non_composite_section = true;
            }
        }
        supported = !(has_composite_section && has_non_composite_section);

        summary[instance_name]["element_types"] = element_types;
        summary[instance_name]["section_categories"] = section_categories;
        summary[instance_name]["supported"] = supported;
        summary[instance_name]["composite"] = has_composite_section;
    }

    std::cout << "done.\n" << std::flush;

    return summary;
}

}  // namespace otk
