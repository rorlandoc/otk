#include "otk/odb.hpp"

#include <fmt/format.h>

#include <odb_API.h>
#include <odb_MaterialTypes.h>
#include <odb_SectionTypes.h>

#include <vtkCellArray.h>
#include <vtkHexahedron.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkQuad.h>
#include <vtkUnstructuredGrid.h>
#include <vtkXMLUnstructuredGridWriter.h>

#include <algorithm>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <vector>

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

void Odb::write_vtu() {
    odb_Assembly &root_assembly = odb_->rootAssembly();
    odb_InstanceRepositoryIT instance_iterator(root_assembly.instances());

    vtkNew<vtkXMLUnstructuredGridWriter> writer;

    for (instance_iterator.first(); !instance_iterator.isDone();
         instance_iterator.next()) {
        vtkNew<vtkPoints> points;
        vtkNew<vtkUnstructuredGrid> grid;

        std::unordered_map<int, vtkIdType> node_map;
        std::unordered_map<std::string, const float *> element_map;

        const odb_Instance &instance = instance_iterator.currentValue();
        odb_Enum::odb_DimensionEnum instance_type = instance.embeddedSpace();

        const odb_SequenceNode &instance_nodes = instance.nodes();
        const odb_SequenceElement &instance_elements = instance.elements();

        std::vector<VTKCellType> cell_types = get_cell_types(instance_elements);
        if (cell_types.empty()) {
            std::cout << "No supported cell types found.\n";
            return;
        }

        std::unordered_map<VTKCellType, vtkCellArray *> cells;
        for (auto cell_type : cell_types) {
            cells[cell_type] = vtkCellArray::New();
        }

        get_points_from_nodes(points.GetPointer(), node_map, instance_nodes,
                              instance_type);

        get_cells_from_elements(cells, node_map, instance_elements);

        grid->SetPoints(points);
        for (auto cell_type : cell_types) {
            grid->SetCells(cell_type, cells[cell_type]);
        }

        writer->SetFileName(path_.replace_extension(".vtu").string().c_str());
        writer->SetInputData(grid);
        writer->Write();
    }
}

void get_points_from_nodes(vtkPoints *points,
                           std::unordered_map<int, vtkIdType> &node_map,
                           const odb_SequenceNode &node_sequence,
                           odb_Enum::odb_DimensionEnum instance_type) {
    int num_nodes = node_sequence.size();
    for (int i = 0; i < num_nodes; ++i) {
        const odb_Node &node = node_sequence[i];
        int node_label = node.label();
        const float *const node_coordinates = node.coordinates();

        if (instance_type == odb_Enum::THREE_D) {
            node_map[node_label] = points->InsertNextPoint(node_coordinates);
        } else if ((instance_type == odb_Enum::TWO_D_PLANAR) ||
                   (instance_type == odb_Enum::AXISYMMETRIC)) {
            node_map[node_label] =
                points->InsertNextPoint(node_coordinates[0], node_coordinates[1], 0.0);
        }
    }
}

std::string get_element_type_base(const std::string &element_type) {
    std::string element_name_base;
    for (auto c : element_type) {
        if (c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5' ||
            c == '6' || c == '7' || c == '8' || c == '9') {
            element_name_base += c;
            break;
        }
        element_name_base += c;
    }
    return element_name_base;
}

std::vector<VTKCellType> get_cell_types(const odb_SequenceElement &element_sequence) {
    std::vector<VTKCellType> cell_types;
    int num_elements = element_sequence.size();
    for (int i = 0; i < num_elements; ++i) {
        const odb_Element &element = element_sequence[i];
        std::string element_type = get_element_type_base(element.type().CStr());

        if (auto it = abq2vtk_cell_map.find(element_type); it != abq2vtk_cell_map.end()) {
            auto v_it = std::find_if(
                cell_types.begin(), cell_types.end(),
                [&](VTKCellType cell_type) { return cell_type == it->second; });
            if (v_it == cell_types.end()) {
                cell_types.push_back(it->second);
            }
        } else {
            std::cout << "Element type " << element_type << " is not supported.\n";
        }
    }
    return cell_types;
}

void get_cells_from_elements(std::unordered_map<VTKCellType, vtkCellArray *> &cells,
                             std::unordered_map<int, vtkIdType> &node_map,
                             const odb_SequenceElement &element_sequence) {
    int num_elements = element_sequence.size();
    int num_nodes = 0;
    for (int i = 0; i < num_elements; ++i) {
        const odb_Element &element = element_sequence[i];
        int element_label = element.label();
        std::string element_type = get_element_type_base(element.type().CStr());
        VTKCellType cell_type;

        if (auto it = abq2vtk_cell_map.find(element_type); it != abq2vtk_cell_map.end()) {
            cell_type = it->second;
        } else {
            std::cout << "Element type " << element_type << " is not supported.\n";
            return;
        }

        const int *const element_connectivity = element.connectivity(num_nodes);

        std::vector<vtkIdType> connectivity(num_nodes);
        for (int i = 0; i < num_nodes; i++) {
            connectivity[i] = node_map[element_connectivity[i]];
        }
        cells[cell_type]->InsertNextCell(connectivity.size(), connectivity.data());
    }
}

}  // namespace otk
