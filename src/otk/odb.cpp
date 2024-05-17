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
//   General info print function
//
// ---------------------------------------------------------------------------------------
void Odb::info() const {
    fmt::print("ODB file info\n\n");
    fmt::print(".. Path: {}\n", path_.string());

    odb_Assembly &root_assembly = odb_->rootAssembly();
    odb_InstanceRepositoryIT instance_iterator(root_assembly.instances());

    fmt::print(".. Number of instances: {}\n\n", root_assembly.instances().size());

    for (instance_iterator.first(); !instance_iterator.isDone();
         instance_iterator.next()) {
        const odb_String &instance_name = instance_iterator.currentKey();
        const odb_Instance &instance = instance_iterator.currentValue();
        odb_Enum::odb_DimensionEnum instance_type = instance.embeddedSpace();
        const odb_SequenceNode &instance_nodes = instance.nodes();
        const odb_SequenceElement &instance_elements = instance.elements();

        int number_nodes = instance_nodes.size();
        int number_elements = instance_elements.size();

        fmt::print(".. Instance: {}\n", instance_name.CStr());
        fmt::print(".... Type: {}\n", static_cast<int>(instance_type));
        fmt::print(".... Number of nodes: {}\n", number_nodes);
        fmt::print(".... Number of elements: {}\n", number_elements);

        for (int i = 0; i < number_elements; ++i) {
            const odb_Element &element = instance_elements[i];
            std::string element_type = element.type().CStr();
            int element_label = element.label();
            int element_index = element.index();

            int n = 0;
            const int *const element_connectivity = element.connectivity(n);

            fmt::print("...... Element: {}\n", i);
            fmt::print("........ Type: {}\n", element_type);
            fmt::print("........ Label: {}\n", element_label);
            fmt::print("........ Index: {}\n", element_index);
            fmt::print("........ Number of nodes: {}\n", n);
            fmt::print("........ Connectivity: ");

            for (int j = 0; j < n; ++j) {
                fmt::print("{} ", element_connectivity[j]);
            }
            fmt::print("\n");
        }
    }
}

void Odb::instances() const {
    fmt::print("OBD instances info\n\n");

    odb_Assembly &root_assembly = odb_->rootAssembly();
    odb_InstanceRepositoryIT instance_iterator(root_assembly.instances());

    for (instance_iterator.first(); !instance_iterator.isDone();
         instance_iterator.next()) {
        const odb_String &instance_name = instance_iterator.currentKey();
        const odb_Instance &instance = instance_iterator.currentValue();

        fmt::print("Instance: {}\n", instance_name.CStr());
        fmt::print(".. Type: {}\n", static_cast<int>(instance.embeddedSpace()));
        fmt::print(".. Number of nodes: {}\n", instance.nodes().size());
        fmt::print(".. Number of elements: {}\n", instance.elements().size());
    }
}

void Odb::nodes(const std::string &instance_name) const {
    fmt::print("ODB nodes info\n\n");

    odb_Assembly &root_assembly = odb_->rootAssembly();
    const odb_Instance &instance =
        root_assembly.instances().constGet(instance_name.c_str());
    odb_Enum::odb_DimensionEnum instance_type = instance.embeddedSpace();
    const odb_SequenceNode &instance_nodes = instance.nodes();

    int num_nodes = instance_nodes.size();

    fmt::print("Number of nodes: {}\n\n", num_nodes);

    for (int i = 0; i < num_nodes; ++i) {
        const odb_Node &node = instance_nodes[i];
        fmt::print("Node: {}\n", i);
        fmt::print(".. Label: {}\n", node.label());
        if (instance_type == odb_Enum::THREE_D) {
            fmt::print(".. Dimension: 3\n");
            fmt::print(".. Coordinates: ({:.6e}, {:.6e}, {:.6e})\n",
                       node.coordinates()[0], node.coordinates()[1],
                       node.coordinates()[2]);
        } else if ((instance_type == odb_Enum::TWO_D_PLANAR) ||
                   (instance_type == odb_Enum::AXISYMMETRIC)) {
            fmt::print(".. Dimension: 2\n");
            fmt::print(".. Coordinates: ({:.6e}, {:.6e})\n", node.coordinates()[0],
                       node.coordinates()[1]);
        }
    }
}

void Odb::elements(const std::string &instance_name) const {
    fmt::print("ODB elements info\n\n");

    odb_Assembly &root_assembly = odb_->rootAssembly();
    const odb_Instance &instance =
        root_assembly.instances().constGet(instance_name.c_str());
    const odb_SequenceElement &instance_elements = instance.elements();

    int num_elements = instance_elements.size();

    fmt::print("Number of elements: {}\n\n", num_elements);

    for (int i = 0; i < num_elements; ++i) {
        const odb_Element &element = instance_elements[i];
        int num_nodes = 0;
        const int *const element_connectivity = element.connectivity(num_nodes);

        fmt::print("Element: {}\n", i);
        fmt::print(".. Type: {}\n", element.type().CStr());
        fmt::print(".. Label: {}\n", element.label());
        fmt::print(".. Index: {}\n", element.index());
        fmt::print(".. Number of nodes: {}\n", num_nodes);
        fmt::print(".. Connectivity: ");

        for (int j = 0; j < num_nodes; ++j) {
            fmt::print("{} ", element_connectivity[j]);
        }
        fmt::print("\n");
    }
}

void Odb::steps() const {
    fmt::print("ODB steps info\n\n");

    odb_StepRepositoryIT step_iterator(odb_->steps());
    int num_steps = odb_->steps().size();

    fmt::print("Number of steps: {}\n\n", num_steps);

    for (step_iterator.first(); !step_iterator.isDone(); step_iterator.next()) {
        const odb_Step &step = step_iterator.currentValue();

        fmt::print("Step: {}\n", step.name().CStr());
        fmt::print(".. Number of frames: {}\n", step.frames().size());
    }
}

void Odb::frames(const std::string &step_name) const {
    fmt::print("ODB frames info\n\n");

    const odb_Step &step = odb_->steps().constGet(step_name.c_str());
    const odb_SequenceFrame &frames = step.frames();
    int num_frames = frames.size();

    fmt::print("Step: {}\n", step_name);
    fmt::print("Number of frames: {}\n\n", num_frames);

    for (int i = 0; i < num_frames; ++i) {
        const odb_Frame &frame = frames[i];

        fmt::print("Frame: {}\n", i);
        fmt::print(".. Id: {}\n", frame.frameId());
        fmt::print(".. Index: {}\n", frame.frameIndex());
        fmt::print(".. Value: {}\n", frame.frameValue());
        fmt::print(".. Increment: {}\n", frame.incrementNumber());
    }
}

void Odb::fields(const std::string &step_name, const int frame_number) const {
    fmt::print("ODB fields info\n\n");

    const odb_Step &step = odb_->steps().constGet(step_name.c_str());
    const odb_Frame &frame = step.frames().constGet(frame_number);
    odb_FieldOutputRepository field_outputs = frame.fieldOutputs();
    odb_FieldOutputRepositoryIT field_output_iterator(field_outputs);

    fmt::print("Step: {}\n", step_name);
    fmt::print("Frame ID: {}\n", frame.frameId());
    fmt::print("Number of field outputs: {}\n\n", field_outputs.size());

    for (field_output_iterator.first(); !field_output_iterator.isDone();
         field_output_iterator.next()) {
        const odb_FieldOutput &field_output = field_output_iterator.currentValue();
        const odb_SequenceFieldLocation &locations = field_output.locations();
        int num_locations = locations.size();

        fmt::print("Field: {}\n", field_output.name().CStr());
        fmt::print(".. Number of blocks: {}\n", field_output.bulkDataBlocks().size());
        fmt::print(".. Has orientation: {}\n", field_output.hasOrientation());
        fmt::print(".. Number of locations: {}\n", num_locations);

        for (int i = 0; i < num_locations; ++i) {
            const odb_FieldLocation &location = locations[i];

            fmt::print(".... Location: {}\n", i);
            fmt::print("...... Position: {}\n", static_cast<int>(location.position()));
            fmt::print("...... Number of section points: {}\n",
                       location.sectionPoint().size());
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
